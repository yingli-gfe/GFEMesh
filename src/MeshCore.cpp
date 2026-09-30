// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "MeshCore.h"
#include "GeomIO.h"
#include "Log.h"
#include "MeshTxtIO.h"
#include "Protocol.h"

#include <BRep_Tool.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <Geom_Curve.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Shape.hxx>

#include <gmsh.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace gfemesh {
namespace {

double edgeLength(const TopoDS_Edge& edge)
{
    double f = 0, l = 0;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(edge, f, l);
    if (curve.IsNull())
        return 0.0;
    BRepAdaptor_Curve adaptor(edge);
    return GCPnts_AbscissaPoint::Length(adaptor);
}

int countFromLength(double length, double size)
{
    if (size <= 0.0)
        return 2;
    int cnt = static_cast<int>(length / size + 1.0 + 0.5);
    return std::max(2, cnt);
}

void applyCurveSizes(const JobFile& job, const TopoDS_Shape& shape)
{
    // gmsh curve tags match TopTools 1-based EDGE indices after
    // importShapesNativePointer.
    TopTools_IndexedMapOfShape edgeMap;
    TopExp::MapShapes(shape, TopAbs_EDGE, edgeMap);

    std::map<int, int> tagToCnt;
    for (int i = 1; i <= edgeMap.Extent(); ++i) {
        const double len = edgeLength(TopoDS::Edge(edgeMap(i)));
        tagToCnt[i] = countFromLength(len, job.defaultSize);
    }

    for (const auto& cs : job.curveSizes) {
        for (int tag : cs.tags) {
            if (cs.density >= 0.0) {
                if (tag < 1 || tag > edgeMap.Extent()) {
                    log().warn("CurveSize tag out of range: " + std::to_string(tag));
                    continue;
                }
                const double len = edgeLength(TopoDS::Edge(edgeMap(tag)));
                tagToCnt[tag] = countFromLength(len, cs.density);
            } else {
                tagToCnt[tag] = std::max(2, cs.cnt);
            }
        }
    }

    for (const auto& [tag, cnt] : tagToCnt)
        gmsh::model::mesh::setTransfiniteCurve(tag, cnt);
}

void applyTransfiniteCurves(const std::vector<int>& tags, const std::vector<int>& cnts)
{
    const std::size_t n = (std::min)(tags.size(), cnts.size());
    for (std::size_t i = 0; i < n; ++i) {
        const int cnt = std::max(2, cnts[i]);
        gmsh::model::mesh::setTransfiniteCurve(tags[i], cnt);
    }
}

void applyTransfiniteSurfaces(const JobFile& job)
{
    for (const auto& s : job.tfSurfaces) {
        applyTransfiniteCurves(s.curveTags, s.curveCnt);
        if (s.corners.empty()) {
            gmsh::model::mesh::setTransfiniteSurface(s.tag);
        } else {
            const std::string arrangement = s.arrangement.empty() ? "Left" : s.arrangement;
            gmsh::model::mesh::setTransfiniteSurface(s.tag, arrangement, s.corners);
        }
    }
}

void applyTransfiniteVolumes(const JobFile& job)
{
    for (const auto& v : job.tfVolumes) {
        applyTransfiniteCurves(v.curveTags, v.curveCnt);
        if (v.corners.empty())
            gmsh::model::mesh::setTransfiniteVolume(v.tag);
        else
            gmsh::model::mesh::setTransfiniteVolume(v.tag, v.corners);
    }
}

void applyRecombine2D(const JobFile& job)
{
    if (job.recombine2d) {
        gmsh::vectorpair faces;
        gmsh::model::getEntities(faces, 2);
        for (const auto& f : faces)
            gmsh::model::mesh::setRecombine(f.first, f.second);
    }
    for (const auto& block : job.recombines) {
        for (int tag : block.tags)
            gmsh::model::mesh::setRecombine(block.dim, tag);
    }
}

void applySweeps(const JobFile& job)
{
    for (const auto& sw : job.sweeps) {
        if (sw.recombine)
            gmsh::model::mesh::setRecombine(sw.source.dim, sw.source.tag);
        const int r = gmsh::model::mesh::gfe::sweep(
            {sw.source.dim, sw.source.tag},
            {sw.body.dim, sw.body.tag},
            {sw.target.dim, sw.target.tag},
            sw.dx, sw.dy, sw.dz,
            sw.layers, sw.ratio,
            sw.recombine);
        if (r != 0)
            log().warn("gfe::sweep returned " + std::to_string(r));
        gmsh::model::occ::synchronize();
    }
}

void applyRevolves(const JobFile& job)
{
    for (const auto& rv : job.revolves) {
        if (rv.recombine)
            gmsh::model::mesh::setRecombine(rv.source.dim, rv.source.tag);
        const int r = gmsh::model::mesh::gfe::revolve(
            {rv.source.dim, rv.source.tag},
            {rv.body.dim, rv.body.tag},
            {rv.target.dim, rv.target.tag},
            rv.x, rv.y, rv.z,
            rv.ax, rv.ay, rv.az,
            rv.angle,
            rv.layers, rv.ratio,
            rv.recombine,
            rv.copyTarget);
        if (r != 0)
            log().warn("gfe::revolve returned " + std::to_string(r));
        gmsh::model::occ::synchronize();
    }
}

} // namespace

int runMeshJob(const JobFile& job, const std::string& outPath)
{
    TopoDS_Shape shape;
    if (const int ec = loadGeometry(job.resolvedGeomPath, job.geomFormat, shape); ec != kExitSuccess)
        return ec;

    std::vector<std::string> argvStd = {"-noenv"};
    std::vector<char*> argv;
    argv.reserve(argvStd.size());
    for (auto& s : argvStd)
        argv.push_back(s.data());

    try {
        gmsh::initialize(static_cast<int>(argv.size()), argv.data());
    } catch (...) {
        log().error("gmsh::initialize failed");
        return kExitGmshError;
    }

    int exitCode = kExitSuccess;
    try {
        for (const auto& o : job.numberOptions)
            gmsh::option::setNumber(o.first, o.second);
        for (const auto& o : job.stringOptions)
            gmsh::option::setString(o.first, o.second);

        gmsh::model::add("meshgen");
        gmsh::model::setCurrent("meshgen");

        gmsh::vectorpair imported;
        gmsh::model::occ::importShapesNativePointer(&shape, imported, false);
        gmsh::model::occ::synchronize();
        log().info("imported entities: " + std::to_string(imported.size()));

        applyCurveSizes(job, shape);
        applyTransfiniteSurfaces(job);
        applyTransfiniteVolumes(job);
        applyRecombine2D(job);
        applySweeps(job);
        applyRevolves(job);

        gmsh::model::mesh::generate(job.generateDim);
        if (job.optimize)
            gmsh::model::mesh::optimize("");

        exitCode = writeMeshTxt(outPath, shape, job.generateDim);
    } catch (...) {
        std::string err, info;
        try {
            gmsh::logger::getLastError(err);
        } catch (...) {
        }
        log().error("gmsh mesh failed: " + err);
        try {
            gmsh::unlock();
        } catch (...) {
        }
        exitCode = kExitGmshError;
    }

    try {
        gmsh::finalize();
    } catch (...) {
    }
    return exitCode;
}

} // namespace gfemesh

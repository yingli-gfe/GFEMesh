// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "Log.h"
#include "MeshTxtIO.h"
#include "Protocol.h"

#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>
#include <TopoDS_Shape.hxx>

#include <gmsh.h>

#include <fstream>
#include <future>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gfemesh {
namespace {

struct TypePack {
    int mshType = 0;
    int numNodes = 0;
    std::vector<std::size_t> elmTags;
    std::vector<std::size_t> elmNodes;
};

struct EntRaw {
    int dim = 0;
    int tag = 0;
    std::vector<TypePack> packs;
    std::size_t elemCount = 0;
};

struct EntBlock {
    int dim = 0;
    int tag = 0;
    std::vector<std::string> lines;
    std::string error;
};

EntBlock buildEntBlock(const EntRaw& raw,
                       const std::unordered_map<std::size_t, int>& gmshToLocal)
{
    EntBlock block;
    block.dim = raw.dim;
    block.tag = raw.tag;
    block.lines.reserve(raw.elemCount);
    for (const auto& pack : raw.packs) {
        const auto& tags = pack.elmTags;
        const auto& nodes = pack.elmNodes;
        const auto nn = static_cast<std::size_t>(pack.numNodes);
        for (std::size_t e = 0; e < tags.size(); ++e) {
            // elmTags are gmsh element ids
            std::string line = std::to_string(tags[e]) + " " + std::to_string(pack.mshType);
            for (std::size_t k = 0; k < nn; ++k) {
                const std::size_t gtag = nodes[e * nn + k];
                const auto it = gmshToLocal.find(gtag);
                if (it == gmshToLocal.end()) {
                    block.error = "unknown node tag in connectivity";
                    return block;
                }
                line += " ";
                line += std::to_string(it->second);
            }
            block.lines.push_back(std::move(line));
        }
    }
    return block;
}

} // namespace

int writeMeshTxt(const std::string& outPath, const TopoDS_Shape& shape, int generateDim)
{
    std::ofstream out(outPath, std::ios::out | std::ios::trunc);
    if (!out) {
        log().error("cannot write mesh txt: " + outPath);
        return kExitIoError;
    }

    Bnd_Box box;
    BRepBndLib::Add(shape, box);
    box.Enlarge(1.0);
    double xmin = 0, ymin = 0, zmin = 0, xmax = 0, ymax = 0, zmax = 0;
    if (!box.IsVoid())
        box.Get(xmin, ymin, zmin, xmax, ymax, zmax);
    const double cx = 0.5 * (xmin + xmax);
    const double cy = 0.5 * (ymin + ymax);
    const double cz = 0.5 * (zmin + zmax);
    const double dx = 0.5 * (xmax - xmin);
    const double dy = 0.5 * (ymax - ymin);
    const double dz = 0.5 * (zmax - zmin);

    std::vector<std::size_t> nodeTags;
    std::vector<double> coords;
    std::vector<double> parametric;
    gmsh::model::mesh::getNodes(nodeTags, coords, parametric);

    std::unordered_map<std::size_t, int> gmshToLocal;
    gmshToLocal.reserve(nodeTags.size() * 2 + 1);
    for (std::size_t i = 0; i < nodeTags.size(); ++i)
        gmshToLocal[nodeTags[i]] = static_cast<int>(i + 1);

    gmsh::vectorpair entities;
    gmsh::model::getEntities(entities);

    // gmsh API is not thread-safe: fetch element data serially, then format in parallel.
    std::vector<EntRaw> raws;
    raws.reserve(entities.size());
    for (const auto& et : entities) {
        const int dim = et.first;
        const int tag = et.second;
        std::vector<int> types;
        std::vector<std::vector<std::size_t>> elmTags;
        std::vector<std::vector<std::size_t>> elmNodes;
        gmsh::model::mesh::getElements(types, elmTags, elmNodes, dim, tag);
        if (types.empty())
            continue;

        EntRaw raw;
        raw.dim = dim;
        raw.tag = tag;
        raw.packs.reserve(types.size());
        for (std::size_t ti = 0; ti < types.size(); ++ti) {
            const int mshType = types[ti];
            std::string name;
            int edim = 0, order = 0, numNodes = 0, numPrim = 0;
            std::vector<double> localCoord;
            gmsh::model::mesh::getElementProperties(
                mshType, name, edim, order, numNodes, localCoord, numPrim);
            if (numNodes <= 0)
                continue;
            const auto& tags = elmTags[ti];
            const auto& nodes = elmNodes[ti];
            if (tags.empty()
                || nodes.size() != tags.size() * static_cast<std::size_t>(numNodes)) {
                log().error("element/node size mismatch for entity dim=" + std::to_string(dim)
                            + " tag=" + std::to_string(tag));
                return kExitGmshError;
            }
            TypePack pack;
            pack.mshType = mshType;
            pack.numNodes = numNodes;
            pack.elmTags = tags;
            pack.elmNodes = nodes;
            raw.elemCount += tags.size();
            raw.packs.push_back(std::move(pack));
        }
        if (raw.elemCount > 0)
            raws.push_back(std::move(raw));
    }

    std::size_t elementCount = 0;
    for (const auto& r : raws)
        elementCount += r.elemCount;

    std::vector<EntBlock> blocks(raws.size());
    std::vector<std::future<void>> tasks;
    tasks.reserve(raws.size());
    for (std::size_t i = 0; i < raws.size(); ++i) {
        tasks.push_back(std::async(std::launch::async, [&, i]() {
            blocks[i] = buildEntBlock(raws[i], gmshToLocal);
        }));
    }
    for (auto& t : tasks)
        t.get();

    for (const auto& b : blocks) {
        if (!b.error.empty()) {
            log().error(b.error);
            return kExitGmshError;
        }
    }

    out << "FormatVersion=" << kMeshTxtFormatVersion << '\n';
    out << "BBoxCen=" << cx << ',' << cy << ',' << cz << '\n';
    out << "BBoxDim=" << dx << ',' << dy << ',' << dz << '\n';
    out << "NodeCount=" << nodeTags.size() << '\n';
    out << "EntityCount=" << raws.size() << '\n';
    out << "GenerateDim=" << generateDim << '\n';
    out << '\n';
    out << "[Nodes]\n";
    for (std::size_t i = 0; i < nodeTags.size(); ++i) {
        out << (i + 1) << ' ' << coords[3 * i] << ' ' << coords[3 * i + 1] << ' '
            << coords[3 * i + 2] << '\n';
    }
    out << '\n';
    for (const auto& b : blocks) {
        out << "[Entity dim=" << b.dim << " tag=" << b.tag << "]\n";
        for (const auto& line : b.lines)
            out << line << '\n';
        out << '\n';
    }

    if (!out) {
        log().error("failed while writing mesh txt: " + outPath);
        return kExitIoError;
    }
    log().info("wrote mesh: nodes=" + std::to_string(nodeTags.size())
               + " elements=" + std::to_string(elementCount)
               + " entities=" + std::to_string(raws.size()));
    return kExitSuccess;
}

} // namespace gfemesh

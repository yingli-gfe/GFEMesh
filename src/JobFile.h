#ifndef GFEMESH_JOBFILE_H
#define GFEMESH_JOBFILE_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace gfemesh {

struct DimTag {
    int dim = -1;
    int tag = -1;
};

struct CurveSizeBlock {
    std::string name;
    std::vector<int> tags;		// for curves, its dims are always 1
    int cnt = 0;
    double density = -1.0;
};

struct SweepBlock {
    DimTag source, target, body;
    double dx = 0, dy = 0, dz = 0;
    std::vector<int> layers;
    std::vector<double> ratio;
    bool recombine = false;
};

struct RevolveBlock {
    DimTag source, target, body;
    double x = 0, y = 0, z = 0;
    double ax = 0, ay = 0, az = 1;
    double angle = 0;
    std::vector<int> layers;
    std::vector<double> ratio;
    bool recombine = false;
    bool copyTarget = true;
};

struct TransfiniteSurfaceBlock {
    int tag = -1;
    std::string arrangement;
    std::vector<int> corners;
    //! Parallel arrays: gmsh curve tag → point count (incl. endpoints)
    std::vector<int> curveTags;
    std::vector<int> curveCnt;
};

struct TransfiniteVolumeBlock {
    int tag = -1;
    std::vector<int> corners;
    std::vector<int> curveTags;
    std::vector<int> curveCnt;
};

struct RecombineBlock {
    int dim = 2;
    std::vector<int> tags;
};

struct JobFile {
    int formatVersion = 0;
    std::string geomPath;
    std::string geomFormat; // bintools | step (may be empty → infer)
    int generateDim = 3;
    double defaultSize = 1.0;
    bool optimize = false;
    bool recombine2d = false;

    std::map<std::string, double> numberOptions;
    std::map<std::string, std::string> stringOptions;

    std::vector<CurveSizeBlock> curveSizes;
    std::vector<SweepBlock> sweeps;
    std::vector<RevolveBlock> revolves;
    std::vector<TransfiniteSurfaceBlock> tfSurfaces;
    std::vector<TransfiniteVolumeBlock> tfVolumes;
    std::vector<RecombineBlock> recombines;

    // Absolute path after resolve against job directory
    std::string resolvedGeomPath;
    std::string jobDir;
};

//! Parse job.txt. Returns 0 on success, otherwise Protocol exit code.
int parseJobFile(const std::string& jobPath, JobFile& out);

} // namespace gfemesh

#endif // GFEMESH_JOBFILE_H

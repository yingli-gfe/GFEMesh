// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "JobFile.h"
#include "Log.h"
#include "MeshCore.h"
#include "Protocol.h"

#include <iostream>
#include <string>

namespace {

void printUsage()
{
    std::cout
        << "mesh_gen (GFEMesh) " << GFEMesh_VERSION << "\n"
        << "Usage: mesh_gen --job <job.txt> --out <result.mesh.txt> [--log <path>]\n"
        << "       mesh_gen -j <job.txt> -o <result.mesh.txt> [-l <path>]\n"
        << "JobFormatVersion=" << gfemesh::kJobFormatVersion
        << " MeshTxtFormatVersion=" << gfemesh::kMeshTxtFormatVersion << "\n"
        << "See docs/cli.md, docs/job-format.md, docs/mesh-txt-format.md\n";
}

} // namespace

int main(int argc, char** argv)
{
    std::string jobPath;
    std::string outPath;
    std::string logPath;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto needArg = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                gfemesh::log().error(std::string("missing value for ") + name);
                return nullptr;
            }
            return argv[++i];
        };
        if (a == "--job" || a == "-j") {
            const char* v = needArg("--job");
            if (!v)
                return gfemesh::kExitJobParseError;
            jobPath = v;
        } else if (a == "--out" || a == "-o") {
            const char* v = needArg("--out");
            if (!v)
                return gfemesh::kExitJobParseError;
            outPath = v;
        } else if (a == "--log" || a == "-l") {
            const char* v = needArg("--log");
            if (!v)
                return gfemesh::kExitJobParseError;
            logPath = v;
        } else if (a == "--help" || a == "-h") {
            printUsage();
            return gfemesh::kExitSuccess;
        } else if (a == "--version" || a == "-V") {
            std::cout << "mesh_gen " << GFEMesh_VERSION << "\n";
            return gfemesh::kExitSuccess;
        } else {
            gfemesh::log().error("unknown argument: " + a);
            printUsage();
            return gfemesh::kExitJobParseError;
        }
    }

    if (argc <= 1) {
        printUsage();
        return gfemesh::kExitSuccess;
    }

    gfemesh::log().setLogPath(logPath);

    if (jobPath.empty() || outPath.empty()) {
        gfemesh::log().error("--job and --out are required");
        printUsage();
        return gfemesh::kExitJobParseError;
    }

    gfemesh::JobFile job;
    if (const int ec = gfemesh::parseJobFile(jobPath, job); ec != gfemesh::kExitSuccess)
        return ec;

    gfemesh::log().info("geom=" + job.resolvedGeomPath + " format=" + job.geomFormat
                        + " dim=" + std::to_string(job.generateDim));
    return gfemesh::runMeshJob(job, outPath);
}

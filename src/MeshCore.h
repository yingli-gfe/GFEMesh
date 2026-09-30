#ifndef GFEMESH_MESHCORE_H
#define GFEMESH_MESHCORE_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include "JobFile.h"

#include <string>

namespace gfemesh {

//! Run full mesh pipeline for a parsed job. Writes --out path.
int runMeshJob(const JobFile& job, const std::string& outPath);

} // namespace gfemesh

#endif // GFEMESH_MESHCORE_H

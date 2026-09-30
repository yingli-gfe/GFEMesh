#ifndef GFEMESH_PROTOCOL_H
#define GFEMESH_PROTOCOL_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL
// Shared protocol constants for mesh_gen (docs/job-format.md, docs/mesh-txt-format.md).

namespace gfemesh {

inline constexpr int kJobFormatVersion = 1;
inline constexpr int kMeshTxtFormatVersion = 1;

// Process exit codes (see docs/cli.md).
inline constexpr int kExitSuccess = 0;
inline constexpr int kExitGmshError = -1;
inline constexpr int kExitRuntimeError = -2;
inline constexpr int kExitNoSuchShape = 1;
inline constexpr int kExitMeshExist = 3;
inline constexpr int kExitElementLimit = 4;
inline constexpr int kExitNodeOverflow = 5;
inline constexpr int kExitElementOverflow = 6;
inline constexpr int kExitElementOrder2 = 7;
inline constexpr int kExitElementOrder2_1 = 8;
inline constexpr int kExitJobParseError = 10;
inline constexpr int kExitIoError = 11;

} // namespace gfemesh

#endif // GFEMESH_PROTOCOL_H

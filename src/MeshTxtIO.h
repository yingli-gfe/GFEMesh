#ifndef GFEMESH_MESHTXTIO_H
#define GFEMESH_MESHTXTIO_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include <string>

class TopoDS_Shape;

namespace gfemesh {

//! Dump current gmsh mesh to .mesh.txt (v1). Uses OCC shape only for bbox.
//! Returns Protocol exit code.
int writeMeshTxt(const std::string& outPath, const TopoDS_Shape& shape, int generateDim);

} // namespace gfemesh

#endif // GFEMESH_MESHTXTIO_H

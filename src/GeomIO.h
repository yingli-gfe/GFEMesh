#ifndef GFEMESH_GEOMIO_H
#define GFEMESH_GEOMIO_H

// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2026 GZYL

#include <string>

class TopoDS_Shape;

namespace gfemesh {

//! Load geometry. format: "bintools" | "step". Returns 0 on success.
int loadGeometry(const std::string& path, const std::string& format, TopoDS_Shape& shape);

} // namespace gfemesh

#endif // GFEMESH_GEOMIO_H

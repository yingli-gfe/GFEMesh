# Gmsh customizations in GFEMesh (net vs public 4.15.2)

## Upstream baseline (public)

| Item | Value |
|------|--------|
| Project | [Gmsh](https://gmsh.info) |
| Version | **4.15.2** |
| Public source | [https://gitlab.onelab.info/gmsh/gmsh](https://gitlab.onelab.info/gmsh/gmsh) |
| Ref / tag | `gmsh_4_15_2` |
| License | GPL-2.0-or-later (+ Gmsh linking exception) — see `LICENSE.txt` |
| Docs | https://gmsh.info/doc/texinfo/gmsh.html |

`third_party/gmsh/` = public **4.15.2** plus the **net** deltas below only.
This document is the release description: it does **not** record add-then-remove
history (anything absent from the current tree relative to an intermediate GFE
branch is simply not a published modification).

## Net modifications (release description)

### 1. `gmsh::model::mesh::gfe` API

In `api/gmsh.h` / `src/common/gmsh_gfe.cpp` (built via `src/common/CMakeLists.txt`):

- `sweep(source, volume, target, dx, dy, dz, numElements, heights, recombine)`  
  Attach sweep/extrude meshing attributes on **existing** geometry (no new entities).
- `revolve(source, volume, target, x,y,z, ax,ay,az, angle, numElements, heights, recombine, copyTarget)`  
  Attach revolve meshing attributes on existing geometry (sector-friendly; optional `copyTarget`).

### 2. `gmsh::unlock`

Additional top-level unlock helper (see `api/gmsh.h` / `gmsh.cpp`) used with the GFE workflow.

### 3. OCC I/O helpers

Small extensions in `GModelIO_OCC.h` / `GModelIO_OCC.cpp` supporting the GFE import path.

### 4. Extrusion / region mesh fixes

- `meshGEdgeExtruded.cpp` — stretch / sweep-copy mesh fixes  
- `meshGRegionExtruded.cpp` — related fix  
- `meshGRegion.cpp` — split pyramid elements when needed  

### 5. Minor contrib / build

- `contrib/QuadMeshingTools/qmtCrossField.cpp`, `contrib/domhex/*` — small patches  
- `src/common/CMakeLists.txt` — compile `gmsh_gfe.cpp`  
- `src/common/gmsh.cpp` — wire `gfe` / `unlock`  

## Build

Top-level: `-DGFEMesh_BUILD_GMSH=ON`. See `BUILD.md`.

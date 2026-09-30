# Building GFEMesh

## Prerequisites

- CMake ≥ 3.16
- C++23 compiler (MSVC recommended on Windows)
- OpenCASCADE (OCCT) with this layout (or adjust CMake):
  - includes: `<OCCT_ROOT>/inc`
  - libs: `<OCCT_ROOT>/win64/vc14/lib` (Release) and optionally `libd` (Debug)
  - Pass `-DGFEMesh_OCCT_ROOT=<OCCT_ROOT>`
- Gmsh (choose one):
  - **External prebuilt:** `-DGMSH_ROOT=<GMSH_ROOT>` with `include/`,
    `Release/gmsh.lib|dll`, optional `Debug/` (`GFEMesh_BUILD_GMSH=OFF`)
  - **Vendored:** `-DGFEMesh_BUILD_GMSH=ON` builds `third_party/gmsh`

## Configure (external Gmsh)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DGFEMesh_BUILD_GMSH=OFF ^
  -DGMSH_ROOT=<GMSH_ROOT> ^
  -DGFEMesh_OCCT_ROOT=<OCCT_ROOT>
cmake --build build --config Release --target mesh_gen
```

Output: `build/bin/Release/mesh_gen.exe` (gmsh.dll is copied beside it).

At runtime, OCCT DLLs must be on `PATH` (e.g. `<OCCT_ROOT>/win64/vc14/bin`).

## Configure (vendored Gmsh)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
  -DGFEMesh_BUILD_GMSH=ON ^
  -DGFEMesh_OCCT_ROOT=<OCCT_ROOT>
cmake --build build --config Release --target mesh_gen
```

Vendored MSVC defaults (shared public API DLL): `ENABLE_BUILD_SHARED=ON`,
`ENABLE_BUILD_LIB` / `ENABLE_BUILD_DYNAMIC` / `ENABLE_PRIVATE_API` / `ENABLE_OPENMP`
all `OFF`. (`DYNAMIC=ON` can hit MSVC LNK1189; classic `/openmp` can hit C7660.)

## Smoke test (box)

```bat
cmake --build build --config Release --target MakeBox
build\bin\Release\MakeBox.exe examples\box.step
build\bin\Release\mesh_gen.exe --job examples\box.job.txt --out examples\box.out.mesh.txt --log examples\box.run.log
```

## Source of Gmsh tree

`third_party/gmsh/` is based on **public Gmsh 4.15.2**
([gmsh.info](https://gmsh.info),
[gitlab.onelab.info/gmsh/gmsh](https://gitlab.onelab.info/gmsh/gmsh),
ref `gmsh_4_15_2`), plus GFE `gfe::sweep` / `gfe::revolve` and related fixes.
See `third_party/gmsh/README.GFE.md`.

## GPL corresponding source

Distributing `mesh_gen.exe` linked with Gmsh requires providing this repository
(including `third_party/gmsh`) for the same revision. See
`docs/gpl-compliance.md`.

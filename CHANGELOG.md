# Changelog

## Unreleased

- Initial repository skeleton for GFEMesh (LGPL-2.1-or-later).
- Vendored **public Gmsh 4.15.2** under `third_party/gmsh/`
  (https://gitlab.onelab.info/gmsh/gmsh), plus **net** GFE deltas
  (`gfe::sweep` / `gfe::revolve`, unlock, OCC/extrusion fixes — see
  `third_party/gmsh/README.GFE.md`).
- Frozen protocol **v1**: `docs/job-format.md`, `docs/mesh-txt-format.md`,
  `docs/cli.md`, and `src/Protocol.h` (format versions + exit codes).
- Examples: `examples/box.job.txt`, `examples/sample.mesh.txt`,
  `examples/tools/make_box.cpp` (optional STEP writer for smoke tests).
- `mesh_gen` CLI + MeshCore: parse job, load STEP/BinTools, drive gmsh
  (curve sizes / Sweep / Revolve / generate / optimize), write `.mesh.txt`.
  CMake links OCCT + external or vendored gmsh.

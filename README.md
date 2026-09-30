# GFEMesh

**GFEMesh** is a standalone mesh tool: it reads a text job (`job.txt`), configures
**Gmsh** from that job, loads an **Open CASCADE `TopoDS_Shape`** (STEP or BinTools),
and writes a text mesh (`.mesh.txt`).

It drives **Gmsh 4.15.2** ([public upstream](https://gmsh.info)) with GFE
`sweep` / `revolve` extensions. The shipped binary is `mesh_gen.exe`.

| Item | Value |
|------|--------|
| Own code license | **LGPL-2.1-or-later** (`LICENSE`, `NOTICE`) |
| Vendored Gmsh | **GPL-2.0-or-later** (+ Gmsh exception) in `third_party/gmsh/` |
| Binary name | `mesh_gen.exe` |
| Input | single `job.txt` (geometry path + explicit mesh options) |
| Output | `.mesh.txt` (nodes + per-entity elements) |

## Layout

```text
GFEMesh/
├── LICENSE / NOTICE / THIRD_PARTY.md / AUTHORS / CHANGELOG.md
├── licenses/gmsh-LICENSE.txt
├── third_party/gmsh/          # full customized Gmsh sources
├── docs/                      # job / mesh.txt / GPL compliance notes
├── src/                       # mesh_gen CLI + MeshCore
├── examples/
├── test/
└── CMakeLists.txt
```

## Build

See [BUILD.md](BUILD.md). Protocols are under `docs/` (v1 frozen).

## Documentation

| Doc | Content |
|-----|---------|
| [docs/cli.md](docs/cli.md) | CLI flags |
| [docs/job-format.md](docs/job-format.md) | **job.txt v1** (frozen) |
| [docs/mesh-txt-format.md](docs/mesh-txt-format.md) | **.mesh.txt v1** (frozen) |
| [docs/gpl-compliance.md](docs/gpl-compliance.md) | License shipping notes |
| [third_party/gmsh/README.GFE.md](third_party/gmsh/README.GFE.md) | Net Gmsh deltas vs public 4.15.2 |

Constants: `src/Protocol.h` (`JobFormatVersion` / mesh format / exit codes).

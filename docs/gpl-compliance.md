# GPL compliance notes (mesh_gen.exe + Gmsh)

## What this project licenses how

| Part | License |
|------|---------|
| GFEMesh own code (`src/`, CMake, docs) | LGPL-2.1-or-later |
| `third_party/gmsh/` | GPL-2.0-or-later (+ Gmsh exception in its LICENSE.txt) |

## When shipping binaries

1. Ship `LICENSE`, `NOTICE`, `THIRD_PARTY.md`, and `licenses/gmsh-LICENSE.txt`
   with `mesh_gen.exe` and the Gmsh runtime library.
2. Provide **complete corresponding source** for the same revision:
   this entire repository including `third_party/gmsh/`, or a written offer
   valid for at least three years (GPL §3).

## Source provenance

`third_party/gmsh/` tracks **public Gmsh 4.15.2**
(https://gmsh.info / https://gitlab.onelab.info/gmsh/gmsh, ref `gmsh_4_15_2`)
plus documented **net** GFE customizations (see `third_party/gmsh/README.GFE.md`).
Cite the public upstream URL/tag above as the baseline for 4.15.2.

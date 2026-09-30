# Third-party components

| Component | Location / use | License | Public source |
|-----------|----------------|---------|---------------|
| Gmsh **4.15.2** + GFE patches | `third_party/gmsh/` (built and linked by mesh_gen.exe) | GPL-2.0-or-later (+ Gmsh exception) | https://gmsh.info — sources: https://gitlab.onelab.info/gmsh/gmsh (tag/`gmsh_4_15_2`) |
| OpenCASCADE (OCCT) | External dependency (`OpenCASCADE_DIR`) | LGPL | https://dev.opencascade.org |

Full license texts:

- `licenses/gmsh-LICENSE.txt` (copy of `third_party/gmsh/LICENSE.txt`)
- Project own code: `LICENSE` (LGPL-2.1)

Customization details (**net** vs public 4.15.2 only): `third_party/gmsh/README.GFE.md`.

# .mesh.txt format

Draft placeholder. Full specification will be added with MeshTxtIO.

Intended shape:

```text
FormatVersion=1
BBoxCen=...
BBoxDim=...

[Nodes]
nid x y z
...

[Entity dim=<d> tag=<t>]
eid gmshElementType n1 n2 ...
...
```

Per-entity sections list **elements only** (no explicit node-id lists).
Entity keys use **gmsh (dim, tag)**.

# job.txt format

`JobFormatVersion=1`. mesh_gen.exe reads explicit sections only.

## Header

| Key | Meaning |
|-----|---------|
| `JobFormatVersion` | `1` |
| `GeomPath` | Absolute path to geometry file |
| `GeomFormat` | `bintools` \| `step` |
| `GenerateDim` | Mesh dimension (typically `3` or `2`) |
| `GFE.DefaultSize` / `Optimize` / `Recombine2D` | Tool options consumed by mesh_gen |
| `Mesh.*` / `General.*` | Passed to gmsh options |

## Sections

### `[CurveSize]`

Global / default curve sizes: `tags=` + `cnt=` (or `density=`).

Long CSV values are written 16 items per line; wrap after a trailing `,`, next line continues (no backslash):

```text
tags=1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
17, 18, 19
cnt=5
```

### `[Sweep]`

Explicit structured sweep → `gmsh::model::mesh::gfe::sweep`. One section per
sweep; applied in file order (each followed by `occ::synchronize`).

Entity ids are gmsh `(dim, tag)` (aligned with OCC ShapeMap on the BinTools path).
Format: `dim,tag`.

```text
[Sweep]
source=2,10
target=2,11
body=3,1
dx=0
dy=0
dz=5
layers=4
ratio=1
recombine=true
```

| Key | Type | Required | Meaning |
|-----|------|----------|---------|
| `source` | `dim,tag` | **yes** | Source face/entity |
| `target` | `dim,tag` | **yes** | Target face/entity |
| `body` | `dim,tag` | **yes** | Containing volume/body |
| `dx` / `dy` / `dz` | double | no (default 0) | Translation vector of the sweep |
| `layers` | int CSV | **yes** (non-empty) | Element counts along the sweep (`numElements`); parallel to `ratio` |
| `ratio` | double CSV | **yes** (non-empty) | Relative heights / spacing weights (same length as `layers` in practice) |
| `recombine` | bool | no (default false) | `true`/`false`/`1`/`0`; recombine source then structured cells |

CSV lists may wrap after a trailing `,` (same rule as `[CurveSize]`).

### `[Revolve]`

Explicit structured revolve → `gmsh::model::mesh::gfe::revolve`. One section per
revolve; applied in file order with `occ::synchronize` after each.

```text
[Revolve]
source=2,3
target=2,4
body=3,2
x=0
y=0
z=0
ax=0
ay=0
az=1
angle=1.5707963267948966
layers=8
ratio=1
recombine=true
copyTarget=true
```

| Key | Type | Required | Meaning |
|-----|------|----------|---------|
| `source` | `dim,tag` | **yes** | Source face/entity |
| `target` | `dim,tag` | **yes** | Target face/entity |
| `body` | `dim,tag` | **yes** | Containing volume/body |
| `x` / `y` / `z` | double | no (default 0) | A point on the revolution axis |
| `ax` / `ay` / `az` | double | no (default `0,0,1`) | Axis direction |
| `angle` | double | no (default 0) | Revolution angle in **radians** |
| `layers` | int CSV | **yes** (non-empty) | Element counts along the angular direction |
| `ratio` | double CSV | **yes** (non-empty) | Spacing weights parallel to `layers` |
| `recombine` | bool | no (default false) | Same meaning as Sweep |
| `copyTarget` | bool | no (default **true**) | Whether to treat/copy the target in the revolve attributes |

CSV wrap rules same as Sweep.

### `[TransfiniteSurface]`

```text
[TransfiniteSurface]
tag=1
arrangement=Left
corners=1,2,3,4
curveTags=10,11,12,13
curveCnt=5,5,8,8
```

- `curveTags` / `curveCnt`: parallel lists; cnt = gmsh point count **including endpoints** (min 2).
- Exe applies `setTransfiniteCurve` for each pair **before** `setTransfiniteSurface`.
- These per-edge counts are authoritative for the structured face; do not rely on `[CurveSize]` alone for Transfinite.

### `[TransfiniteVolume]`

```text
[TransfiniteVolume]
tag=1
corners=...
curveTags=...
curveCnt=...
```

Same `curveTags`/`curveCnt` rule; curves applied before `setTransfiniteVolume`.

### `[Recombine]`

Optional face tags for 2D recombine (or driven by `GFE.Recombine2D`).

# mesh_gen.exe CLI (v1)

```text
mesh_gen.exe --job <job.txt> --out <result.mesh.txt> [--log <path>]
```

| Argument | Required | Description |
|----------|----------|-------------|
| `--job` | yes | Path to job file — see [job-format.md](job-format.md) |
| `--out` | yes | Path to write `.mesh.txt` — see [mesh-txt-format.md](mesh-txt-format.md) |
| `--log` | no | Optional log file (mirrors console diagnostics) |

Short aliases: `-j`, `-o`, `-l`.

On success: exit `0`, `--out` file created/overwritten.  
On failure: non-zero exit (see `src/Protocol.h`), `--out` may be absent or incomplete;
details go to stderr / `--log`.

`INFO` goes to stdout; `WARN` / `ERROR` go to stderr. Mesh data is only in `--out`,
not on stdout.

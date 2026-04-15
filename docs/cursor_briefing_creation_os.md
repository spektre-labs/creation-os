# Cursor briefing — Creation OS

## Mission

Repository: [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os). **One command:** `make all`. **One file to read first:** `creation_os_v2.c`.

## Invariants

1. `make test` and `make bench` must pass before a release tag.  
2. Do not commit weights, `.zip` blobs, or `artifacts/` into this tree.  
3. Benchmark claims in `README.md` must stay tied to **measured** `make bench` output or to the **fixed** 32× / 192× proxies from `D=4096`, `W=64`.

## Layout

| Path | Role |
|------|------|
| `creation_os_v2.c` | Full §1–§26 reference program |
| `core/cos_bsc.h` | Shared HV primitives for split `.c` files |
| `tests/test_bsc_core.c` | σ / MAJ / XOR / ROTL / Noether / Crystal lock |
| `core/gemm_vs_bsc.c` | `cos_gemm_bench_main()` — linked by `bench/gemm_vs_bsc.c` |

## Publishing

Use [publish_checklist_creation_os.md](publish_checklist_creation_os.md): verify `make check`, then push **this** tree to **creation-os** `main`.

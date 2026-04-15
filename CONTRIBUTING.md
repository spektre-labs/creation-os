# Contributing to Creation OS (`creation-os`)

Thank you for improving the kernel, tests, or documentation. **Repository prose is English-only** (issues and PR descriptions may use other languages if your team agrees; **merged docs stay English**).

## Before you open a PR

1. **Read** [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md) if your change touches benchmarks, numbers, or comparisons to frontier models.
2. **Run** from this directory:
   ```bash
   make check
   ```
   This builds `creation_os` from `creation_os_v2.c` and runs `tests/test_bsc_core.c`.
3. If you change **reported throughput** or tables in the README, attach or describe a **repro bundle** per [docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md).

## Build targets

| Target | Purpose |
|--------|---------|
| `make help` | List targets |
| `make standalone` | Single-file demo binary |
| `make test` | σ / Noether / crystal structural tests |
| `make bench` | GEMM vs BSC microbench (prints host-dependent rates) |
| `make check` | `standalone` + `test` (CI default) |
| `make all` | `standalone`, `oracle`, `bench`, `physics`, `test` |

## C code

- **Standard:** C11 (`-std=c11`).
- **License:** New files under this tree should carry SPDX `AGPL-3.0-or-later` and the project’s copyright notice (match headers in `creation_os_v2.c` / `tests/`).
- **Style:** Match surrounding files; avoid drive-by reformat of unrelated lines.

## README policy

Do **not** delete or contradict existing **Limitations**, **Measured results** footnotes, or **Publication-hard** commitments without maintainer review. Prefer **additive** clarifications and links to `docs/`.

## Security

See [SECURITY.md](SECURITY.md).

---

*Spektre Labs · Creation OS · 2026*

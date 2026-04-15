# Contributing to Creation OS (`creation-os`)

Thank you for improving the kernel, tests, or documentation. **All committed material is English only** — see [docs/LANGUAGE_POLICY.md](docs/LANGUAGE_POLICY.md). Thread-level FAQ for misreads: [docs/COMMON_MISREADINGS.md](docs/COMMON_MISREADINGS.md).

## Before you open a PR

1. **Read** [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md) if your change touches benchmarks, numbers, or comparisons to frontier models. For §7 specifics see [docs/BENCHMARK_PROTOCOL.md](docs/BENCHMARK_PROTOCOL.md); for shared vocabulary see [docs/GLOSSARY.md](docs/GLOSSARY.md).
2. **Run** from this directory before opening a PR:
   ```bash
   make merge-gate
   ```
   This runs **`make check`** (portable `creation_os` + `test_bsc_core`) and **`make check-v6` … `make check-v26`** (every flagship `--self-test`, including **204** checks on v26). Same command as CI and **`make push-main`** preflight. While iterating on a single `creation_os_vN.c`, you may run only **`make check-vN`** until the final rebase, then **`make merge-gate`** once.
3. If you change **reported throughput** or tables in the README, attach or describe a **repro bundle** per [docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md).

## Build targets

| Target | Purpose |
|--------|---------|
| `make help` | List targets |
| `make standalone` | Single-file demo binary |
| `make test` | σ / Noether / crystal structural tests |
| `make bench` | GEMM vs BSC microbench (prints host-dependent rates) |
| `make bench-coherence` | Batch Hamming coherence gate (NEON on AArch64) |
| `make bench-agi-gate` | Parliament (odd K) + memory-bank argmin bench |
| `make check` | `standalone` + `test` (portable kernel gate) |
| `make merge-gate` | Full matrix: `check` + `check-v6` … `check-v26` (CI / publish / PR-ready) |
| `make check-v6` | `creation_os_v6.c` + `--self-test` (30 checks) |
| `make check-v7` | `creation_os_v7.c` + `--self-test` (35 checks) |
| `make check-v9` | `creation_os_v9.c` + `--self-test` (41 checks) |
| `make check-v10` | `creation_os_v10.c` + `--self-test` (46 checks) |
| `make check-v11` | `creation_os_v11.c` + `--self-test` (49 checks) |
| `make check-v12` | `creation_os_v12.c` + `--self-test` (52 checks) |
| `make check-v15` … `make check-v26` | v15 Silicon mind through v26 G500 echo orbit (`--self-test`; exact counts in `make help`) |
| `make all` | `standalone`, `oracle`, `bench`, `physics`, `test` |

## C code

- **Standard:** C11 (`-std=c11`).
- **License:** New files under this tree should carry SPDX `AGPL-3.0-or-later` and the project’s copyright notice (match headers in `creation_os_v2.c` / `tests/`).
- **Style:** Match surrounding files; avoid bulk reformat of unrelated lines.

## README policy

Do **not** delete or contradict existing **Limitations**, **Measured results** footnotes, or **Publication-hard** commitments without maintainer review. Prefer **additive** clarifications and links to `docs/`.

## Security

See [SECURITY.md](SECURITY.md). Maintainer-oriented notes: [docs/SECURITY_DEVELOPER_NOTES.md](docs/SECURITY_DEVELOPER_NOTES.md).

## Publishing this tree to GitHub

Maintainers: [docs/MAINTAINERS.md](docs/MAINTAINERS.md) — push product **`main`** with **`make push-main`** (runs `merge-gate` then publishes to **spektre-labs/creation-os**; same as `make publish-github`).

---

*Spektre Labs · Creation OS · 2026*

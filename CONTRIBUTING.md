# Contributing to Creation OS (`creation-os`)

Thank you for improving the kernel, tests, or documentation. **All committed material is English only** — see [docs/LANGUAGE_POLICY.md](docs/LANGUAGE_POLICY.md). Thread-level FAQ for misreads: [docs/COMMON_MISREADINGS.md](docs/COMMON_MISREADINGS.md).

## Before you open a PR

1. **Read** [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md) if your change touches benchmarks, numbers, or comparisons to frontier models. For §7 specifics see [docs/BENCHMARK_PROTOCOL.md](docs/BENCHMARK_PROTOCOL.md); for shared vocabulary see [docs/GLOSSARY.md](docs/GLOSSARY.md).
2. **Run** from this directory before opening a PR:
   ```bash
   make merge-gate
   ```
   This runs **`make check`** (portable `creation_os` + `tests/test_bsc_core`) and **`make check-v6` … `make check-v29`** (every flagship `--self-test` in the merge matrix — e.g. **184** checks on v26, **70** on v27, **29** on v28, **22** on v29). Same command as CI and `make publish-github` preflight. **σ labs (v31+, MCP, HDL)** are optional; see README [σ labs (v31–v40)](README.md#sigma-labs-v31-v40) and `make help`. While iterating on a single `creation_os_vN.c`, you may run only **`make check-vN`** until the final rebase, then **`make merge-gate`** once. If you touch **`rtl/*.sv`**, **`hw/chisel/**`**, or **`hw/rust/spektre-iron-gate`**, also run **`make stack-ultimate`** and **`make rust-iron-lint`** (Verilator + Yosys + Rust; Chisel steps SKIP without sbt).
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
| `make merge-gate` | Full matrix: `check` + `check-v6` … `check-v29` (CI / publish / PR-ready) |
| `make check-v6` | `creation_os_v6.c` + `--self-test` (30 checks) |
| `make check-v7` | `creation_os_v7.c` + `--self-test` (35 checks) |
| `make check-v9` | `creation_os_v9.c` + `--self-test` (41 checks) |
| `make check-v10` | `creation_os_v10.c` + `--self-test` (46 checks) |
| `make check-v11` | `creation_os_v11.c` + `--self-test` (49 checks) |
| `make check-v12` | `creation_os_v12.c` + `--self-test` (52 checks) |
| `make check-v15` … `make check-v26` | v15 Silicon mind through v26 G500 echo orbit (`--self-test`; exact counts in `make help`) |
| `make check-v27` … `make check-v29` | Tokenizer scaffold (v27), LM integration shell (v28), collapse harness (v29) — merge-gate includes these |
| `make check-v31` / `check-v33`–`v45`, `check-mcp` | Optional σ / agent / MCP labs — **not** merge-gate |
| `make formal-rtl-lint` | Verilator lint on `rtl/*.sv` |
| `make stack-ultimate` | Lint + Yosys elab + SAT prove + Verilator sim + Rust iron + Chisel (SKIPs OK) |
| `make rust-iron-lint` | `cargo fmt --check` + `clippy -D warnings` on iron gate |
| `make stack-nucleon` / `make stack-singularity` | SymbiYosys + EQY layers (see `hw/formal/README.md`) |
| `make all` | `standalone`, `oracle`, `bench`, `physics`, `test` |

## C code

- **Standard:** C11 (`-std=c11`).
- **License:** New files under this tree should carry SPDX `AGPL-3.0-or-later` and the project’s copyright notice (match headers in `creation_os_v2.c` / `tests/`).
- **Style:** Match surrounding files; avoid bulk reformat of unrelated lines.

## README policy

Do **not** delete or contradict existing **Limitations**, **Measured results** footnotes, or **Publication-hard** commitments without maintainer review. Prefer **additive** clarifications and links to `docs/`.

## Security

See [SECURITY.md](SECURITY.md). Maintainer-oriented notes: [docs/SECURITY_DEVELOPER_NOTES.md](docs/SECURITY_DEVELOPER_NOTES.md).

## Local disk hygiene (macOS)

If Finder created duplicate trees (`README 2.md`, `core 2/`, …), remove them from the repo root with:

```bash
bash scripts/prune_finder_duplicates.sh
```

Those paths are **gitignored** and must never be committed.

## Publishing this tree to GitHub

Maintainers: [docs/MAINTAINERS.md](docs/MAINTAINERS.md) (`make publish-github` after `make merge-gate`).

---

*Spektre Labs · Creation OS · 2026*

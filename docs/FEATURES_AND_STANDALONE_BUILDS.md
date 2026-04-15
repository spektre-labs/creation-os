# Features and standalone builds

This page is the **single map** of what exists in this portable **Creation OS** tree: which C programs ship here, what each one is for, how to verify it, and where to read scope and non-claims.

**Evidence class reminder:** everything below except **`creation_os` + `make test`** is **lab demo / schematic C** unless you add external benchmark evidence per [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

---

## Quick matrix

| Program (binary) | Source file | Purpose (one line) | Verify | Self-test count | Canonical doc |
|------------------|---------------|--------------------|--------|------------------|-----------------|
| `creation_os` | `creation_os_v2.c` | 4096-bit BSC reference kernel, 26 modules, Oracle / bench demos | `make check` | via `test_bsc` (structural + NEON parity where applicable) | README, [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md) |
| `creation_os_v6` | `creation_os_v6.c` | σ–K–L–S scaffold + M01–M18 narrative “Living Kernel” toys | `make check-v6` | 30 | [LIVING_KERNEL_V6.md](LIVING_KERNEL_V6.md) |
| `creation_os_v7` | `creation_os_v7.c` | v6 + M19–M23 hallucination-shaped σ readouts | `make check-v7` | 35 | [HALLUCINATION_KILLER_V7.md](HALLUCINATION_KILLER_V7.md) |
| `creation_os_v9` | `creation_os_v9.c` | v7 + M24–M29 stack / silicon-shaped σ schematics | `make check-v9` | 41 | [PARAMETERS_IN_SILICON_V9.md](PARAMETERS_IN_SILICON_V9.md) |
| `creation_os_v10` | `creation_os_v10.c` | v9 + M30–M33 distillation / few-shot / swarm / abstention toys | `make check-v10` | 46 | [THE_REAL_MIND_V10.md](THE_REAL_MIND_V10.md) |
| `creation_os_v11` | `creation_os_v11.c` | v10 + M34 matmul-free LM toy (ternary BitLinear + MLGRU forward) | `make check-v11` | 49 | [THE_MATMUL_FREE_MIND_V11.md](THE_MATMUL_FREE_MIND_V11.md) |
| `creation_os_v12` | `creation_os_v12.c` | v11 + M35–M37 classical MPS / entanglement-meter / TN-sequence toys | `make check-v12` | 52 | [THE_TENSOR_MIND_V12.md](THE_TENSOR_MIND_V12.md) |

---

## Module IDs by file (high level)

| File | Module range | Theme |
|------|----------------|-------|
| `creation_os_v2.c` | §1–§26 (see README) | BSC core, Hypercube, Oracle, Soul, Proconductor, JEPA toy, … |
| `creation_os_v6.c` | M01–M18 | RDP, alignment tax, preference collapse, RAIN-style σ-tape, ghost boot, Gödel-boundary toy, … |
| `creation_os_v7.c` | M01–M23 | All of v6 + anchor collapse, association ratio, bluff σ, context rot, JEPA–Oracle |
| `creation_os_v9.c` | M01–M29 | All of v7 + neuromorphic/CIM/memory-wall/BNN/silicon-stack/hetero **schematics** |
| `creation_os_v10.c` | M01–M33 | All of v9 + distilled-mind toy, prototypical few-shot, specialist swarm, σ-aware abstention gate |
| `creation_os_v11.c` | M01–M34 | All of v10 + matmul-free language-model schematic (M34) |
| `creation_os_v12.c` | M01–M37 | All of v11 + MPS encoder, entanglement σ-meter, TN sequence head (toys) |

---

## CI and publish

- **CI** (`.github/workflows/ci.yml`): `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11 && make check-v12`.
- **Publish** (`tools/publish_to_creation_os_github.sh`): same checks before rsync; strips all listed binaries so they are never committed upstream.

---

## Misreadings to avoid

See [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) — especially rows for v6–v12 so schematic demos are not confused with harness scores, silicon tape-out, quantum hardware claims, or “real AGI.”

---

*Spektre Labs · Creation OS · 2026*

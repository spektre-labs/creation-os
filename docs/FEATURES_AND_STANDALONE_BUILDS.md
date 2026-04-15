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
| `creation_os_v21` | `creation_os_v21.c` | v20 ship mode + M65–M76 AGI sovereign stack (governance schematic, §45h) | `make check-v21` | 99 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |
| `creation_os_v22` | `creation_os_v22.c` | v21 + M77–M96 twenty colossal insights (2025 safety / verification web anchors, §45i) | `make check-v22` | 120 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |
| `creation_os_v23` | `creation_os_v23.c` | v22 + M97–M116 AGI-schematic affordances (2026 agent / memory / GUI discourse hooks, §45j) | `make check-v23` | 141 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |
| `creation_os_v24` | `creation_os_v24.c` | v23 + M117–M136 arXiv sci-fi echo latches (real preprint IDs, §45k) | `make check-v24` | 162 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |
| `creation_os_v25` | `creation_os_v25.c` | v24 + M137–M156 enterprise pain → mitigation ledger (OWASP LLM + EU AI Act routing, §45m) | `make check-v25` | 183 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |
| `creation_os_v26` | `creation_os_v26.c` | v25 + M157–M176 Fortune Global 500–scale echo orbit (journalism themes → M45–M116 hooks, §45n) | `make check-v26` | 204 | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), README |

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
| `creation_os_v21.c` | M01–M76 | v12 lineage through v20 twenty pillars (M45–M64) + sovereign stack hooks (M65–M76) |
| `creation_os_v22.c` | M01–M96 | v21 + M77–M96 insight stack (web-anchored schematic latches) |
| `creation_os_v23.c` | M01–M116 | v22 + M97–M116 AGI-schematic affordances (agent-discourse latches) |
| `creation_os_v24.c` | M01–M136 | v23 + M117–M136 arXiv citation echo latches |
| `creation_os_v25.c` | M01–M156 | v24 + M137–M156 enterprise pain routing ledger |
| `creation_os_v26.c` | M01–M176 | v25 + M157–M176 Global 500 echo orbit (mega-cap theme latches) |

---

## CI and publish

- **CI** (`.github/workflows/ci.yml` on **spektre-labs/creation-os**): `make merge-gate` (portable `check` + `check-v6` … `check-v26`).
- **Push product `main`** (`tools/publish_to_creation_os_github.sh` via **`make push-main`**): `make merge-gate` before rsync; strips listed binaries; pushes only to **https://github.com/spektre-labs/creation-os** (not a parent monorepo).

---

## Misreadings to avoid

See [COMMON_MISREADINGS.md](COMMON_MISREADINGS.md) — especially rows for v6–v12 so schematic demos are not confused with harness scores, silicon tape-out, quantum hardware claims, or “real AGI.”

---

*Spektre Labs · Creation OS · 2026*

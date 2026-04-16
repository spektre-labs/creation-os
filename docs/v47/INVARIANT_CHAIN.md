# Creation OS v47 — invariant chain (honest tiers)

Every layer has a **stated** invariant. Each invariant is tagged **M** (measured / proved in-repo on supported hosts), **T** (tested but not formally proved), or **P** (projected / architecture only).

This document is the v47 “foundation pour”: it tightens **claims** without inventing **proofs**.

## Layer 0: σ-kernel (C, float Dirichlet evidence + softmax entropy helpers)

**Invariant:** On bounded valid inputs, the v47 kernel computes finite Dirichlet decomposition consistent with the shipped `sigma_decompose_dirichlet_evidence` within float tolerance, and returns entropy ≥ 0 for the stable-softmax Shannon sum.

**Proof method:** ACSL contracts + Frama-C/WP + RTE (tooling-dependent).

**Status:** **T** (contracts + `creation_os_v47 --self-test` parity checks). **M** only after `frama-c` is installed and obligations are discharged in CI or a pinned proof report.

## Layer 1: σ-pipeline (SystemVerilog, v37 harness)

**Invariant:** Under the formal harness assumptions (spaced `valid_in` pulses), popcounts stay within bit-width, abstention matches the variance threshold relation encoded in `sigma_pipeline.sv`, and `valid_in |-> ##3 valid_out` holds.

**Proof method:** SymbiYosys BMC (`sby`).

**Status:** **M** when `sby` is present (`make formal-sby-v37` / `make verify-sv`). Extended depth harness: `hdl/v47/sigma_pipeline_extended.sby`.

## Layer 2: σ-proxy (inference layer)

**Invariant:** The proxy path should not emit downstream actions without a σ measurement on the logits stream (product intent).

**Proof method:** asserts + integration tests (where wired).

**Status:** **T** where the v44 proxy lab is exercised (`make check-proxy`); not a full functional-correctness proof.

## Layer 3: σ-routing (fallback)

**Invariant:** When σ-derived signals exceed configured thresholds and a fallback is configured, routing prefers the fallback path (policy intent).

**Proof method:** unit tests + config parsing tests.

**Status:** **T** for the v33 router lab surfaces (`make check-v33`).

## Layer 4: σ-threshold theorem (multi-channel abstention)

**Invariant:** Independent σ-channels reduce hallucination rate under stated toy syndromes (threshold / QEC analogy).

**Proof method:** empirical benchmarks (TruthfulQA-class harness) — not archived here by default.

**Status:** **P** (stubs: `make bench-v40-threshold`).

## Layer 5: σ-calibration (Platt / temperature hooks)

**Invariant:** Calibrated σ tracks empirical correctness probability on held-out data.

**Proof method:** calibration curves on archived eval JSON.

**Status:** **P** (loss + hook code exists; full curves not claimed without artifacts).

## Layer 6: σ-introspection (v45 lab)

**Invariant:** `calibration_gap` stays below a target on a validation slice.

**Proof method:** benchmark JSON + plotting inputs.

**Status:** **P** (`make bench-v45-paradox` stub).

## Layer 7: ZK-verification (v47 API)

**Invariant:** A verifier accepts a proof **iff** σ was computed from the committed logits (soundness), **without** revealing logits (zero-knowledge).

**Proof method:** succinct argument system (SNARK/STARK) over a fixed σ circuit.

**Status:** **P**. The in-tree `src/v47/zk_sigma.c` is a **structural stub**: it packs a short witness into `proof[256]` (≤ 62 floats) and checks digest + recomputation. That is **not** zero-knowledge and **not** a SNARK.

## Verification coverage summary

| Layer | Topic                         | Tier |
|------:|-------------------------------|:----:|
| 0     | C σ-kernel                    | T → M (Frama-C) |
| 1     | SV σ-pipeline                 | M (sby) |
| 2     | σ-proxy                       | T |
| 3     | σ-routing                     | T |
| 4     | threshold / multi-channel   | P |
| 5     | calibration                 | P |
| 6     | introspection gap           | P |
| 7     | ZK-σ                        | P (stub API) |

## One-command stack check

`make verify` runs **best-effort** layers: Frama-C (SKIP if missing), extended `sby`, Hypothesis tests (SKIP if missing), and a portable integration slice (`creation_os_v47` + `check-v31` + `check`). It is **not** identical to `make merge-gate`.

## Target discipline

Move layers **P → T → M** only when the corresponding command + artifact exists and `docs/WHAT_IS_REAL.md` is updated in the same change.

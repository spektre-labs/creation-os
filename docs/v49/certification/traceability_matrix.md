# Traceability Matrix — Requirements ↔ Code ↔ Tests ↔ Proofs (v49 lab)

**Legend:** “Proof” means **Frama-C/WP discharge target** (only **M** when CI/log shows all goals closed). `mcdc_tests.c` is the current unit driver.

| HLR | LLR | Source | Frama-C / formal | Unit / driver | Notes |
|---|---|---|---|---|---|
| HLR-001 | LLR-001a | `v47_verified_softmax_entropy` — `src/v47/sigma_kernel_verified.c` | WP+RTE target | `test_entropy_basic` in `tests/v49/mcdc_tests.c` | Entropy clamped to `>= 0` |
| HLR-002 | LLR-002a | `v47_verified_top_margin01` — `src/v47/sigma_kernel_verified.c` | WP+RTE target | `test_margin_basic` | Clamp to `[0,1]` |
| HLR-003 | LLR-003a | `v47_verified_dirichlet_decompose` — `src/v47/sigma_kernel_verified.c` | WP+RTE target | `test_decomp_sum` | Sum identity when defined |
| HLR-004 | LLR-004a | `v49_sigma_gate_calibrated_epistemic` — `src/v49/sigma_gate.c` | WP+RTE target (small TU) | `test_gate_mcdc` | MC/DC-style paths |
| HLR-005 | LLR-005a | Pure functions in v47 TU + v49 gate | no hidden state (review) | deterministic tests | Not a full product proof |
| HLR-006 | LLR-006a | ACSL + C11 hygiene | `frama-c -wp -wp-rte` (optional) | ASan/UBSan smoke (`binary_audit.sh`) | Tool-discharge is **not** claimed by default |
| HLR-007 | — | complexity | not a proof obligation | **P-tier** bench TBD | See `HLR.md` |
| HLR-008 | LLR-008a | no heap in v47 kernel TU | grep/manifest gate | `verify_traceability.py` | `must_not_contain: malloc` |
| HLR-009 | LLR-004a + kernel guards | gate + safe outputs | partial formal targets | gate tests + kernel tests | See `HLR.md` split |

## Bidirectional trace automation

`python3 scripts/v49/verify_traceability.py` validates `docs/v49/certification/trace_manifest.json`.

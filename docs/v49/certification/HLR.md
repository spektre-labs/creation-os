# σ-kernel + σ-gate — High-Level Requirements (HLR) (v49 lab)

**Non-claim:** These HLRs are **in-repo engineering requirements** for a small critical path. They are **not** FAA/EASA-approved requirements baselines unless an authority says so.

## HLR-001: Entropy computation

The system SHALL compute Shannon entropy from a logits vector of length `1` to `V47_VERIFIED_MAX_N` (131072) using a stable-softmax construction and return a finite, non-negative float.

**Implementation:** `v47_verified_softmax_entropy`

## HLR-002: Top-margin computation

The system SHALL compute a normalized top-1 minus top-2 margin on logits and return a float in `[0.0, 1.0]`.

**Implementation:** `v47_verified_top_margin01`

## HLR-003: Epistemic / aleatoric decomposition

The system SHALL decompose uncertainty into aleatoric and epistemic components using the Dirichlet-evidence interpretation consistent with `src/sigma/decompose.c`, such that `total = aleatoric + epistemic` when the decomposition is defined.

**Implementation:** `v47_verified_dirichlet_decompose`

## HLR-004: Abstention decision (scalar gate)

The system SHALL emit `V49_ACTION_ABSTAIN` if and only if the calibrated epistemic scalar exceeds the configured threshold **or** the inputs are non-finite.

**Implementation:** `v49_sigma_gate_calibrated_epistemic`

> **Note:** “Calibration” itself is policy outside this function; the gate consumes a **scalar already prepared** by upstream logic.

## HLR-005: Determinism

The σ-kernel functions SHALL be pure on their inputs (no hidden mutable state).

**Scope note:** This HLR targets the v47 kernel TU + v49 gate TU, not an entire serving stack.

## HLR-006: No undefined behavior (verification target)

The σ-kernel and σ-gate TUs SHALL not trigger undefined behavior on valid inputs per ISO C11, including out-of-bounds access, invalid pointer use, and invalid floating exceptional cases that the verification plan treats as errors.

**Verification target:** Frama-C/WP + RTE (optional tool), plus ASan/UBSan smoke builds in `binary_audit.sh`.

## HLR-007: Bounded execution time (verification objective)

The system SHOULD complete σ-kernel computation in `O(V)` time where `V` is vocabulary size.

**Tier:** **P** in `WHAT_IS_REAL.md` until a pinned microbench artifact exists for `V ≤ 131072` on a named CPU configuration.

## HLR-008: Memory safety (no heap in kernel TU)

The σ-kernel TU SHALL not allocate heap memory during computation.

**Implementation:** `src/v47/sigma_kernel_verified.c` (checked by `verify_traceability.py` + manual review).

## HLR-009: Fail-closed (split responsibility)

1. **Gate:** Non-finite or over-threshold epistemic SHALL map to `V49_ACTION_ABSTAIN` (`v49_sigma_gate_calibrated_epistemic`).
2. **Kernel:** Invalid / ill-conditioned inputs SHALL yield safe finite outputs (e.g., entropy `0`, decompose cleared) rather than propagating `NaN` where the implementation explicitly guards.

> This is **not** a claim that every upstream failure mode is already wired to abstain in all demos.

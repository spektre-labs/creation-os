# Low-Level Requirements (LLR) — σ-critical path (v49 lab)

## LLR-001a (entropy)

For `1 <= n <= V47_VERIFIED_MAX_N`, `v47_verified_softmax_entropy(logits, n)` SHALL return a finite `H` with `H >= 0`.

## LLR-002a (margin)

For `2 <= n <= V47_VERIFIED_MAX_N`, `v47_verified_top_margin01(logits, n)` SHALL return `m` with `0 <= m <= 1`.

## LLR-003a (decomposition)

For valid `logits` and `n`, `v47_verified_dirichlet_decompose` SHALL write `out->total = out->aleatoric + out->epistemic` when decomposition is defined; otherwise SHALL zero `*out` safely.

## LLR-004a (abstention gate)

`v49_sigma_gate_calibrated_epistemic(e, t)` SHALL return `V49_ACTION_ABSTAIN` if `e` or `t` is non-finite, or if `e > t`; otherwise SHALL return `V49_ACTION_EMIT`.

## LLR-006a (bounded iteration)

All loops in `sigma_kernel_verified.c` SHALL be bounded by `n` with explicit loop variants in ACSL (Frama-C target).

## LLR-008a (no heap in kernel TU)

`src/v47/sigma_kernel_verified.c` SHALL not call `malloc`, `calloc`, `realloc`, or `free`.

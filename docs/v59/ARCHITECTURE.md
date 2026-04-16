# v59 Architecture

## Wire map

```
caller (v34 Dirichlet / v55 σ₃ / v56 VPRM)
        │  per-step {ε, α, answer_delta, reflection_signal}
        ▼
  cos_v59_score_step          scalar, branchless, 3-FMA
  cos_v59_score_soa_neon      NEON 4-accumulator SoA reduction
        │  readiness score
        ▼
  cos_v59_decide_online       branchless 4-way mux
     at_cap                       → EARLY_EXIT
     σ high ∧ α-dom ∧ above_min   → ABSTAIN
     σ high ∧ ¬α-dom              → EXPAND
     readiness ≥ τ ∧ above_min    → EARLY_EXIT
     else                         → CONTINUE
        │
        ▼
  caller inference loop (v53 σ-TAOR harness, v54 proconductor, …)
```

## Slot table (v57)

```
  [M] adaptive_compute_budget    (owner: v59  target: make check-v59)
```

## Tiering

| Claim                                   | Tier |
|-----------------------------------------|------|
| branchless decision kernel              | M    |
| NEON 4-acc SoA score = scalar reference | M    |
| determinism across repeated calls       | M    |
| counts ∑ = n in `summarize_trace`       | M    |
| ε / α fidelity to v34                   | I    |
| formal proof of decision kernel         | —    |
| end-to-end tokens-per-correct           | P    |

## Hardware discipline (.cursorrules mapping)

| Rule (.cursorrules)        | v59 location                                                          |
|----------------------------|-----------------------------------------------------------------------|
| 2. `aligned_alloc(64, …)`  | `cos_v59_alloc_steps` + `v59_alloc64` in driver tests                 |
| 3. prefetch 16 ahead       | `cos_v59_score_batch`, `cos_v59_summarize_trace`, NEON SoA loop       |
| 4. branchless kernel       | `cos_v59_decide_online` (mask AND-NOT cascade, no `if` on hot path)   |
| 5. NEON 4-accumulator      | `cos_v59_score_soa_neon` (s0..s3, three `vfmaq_f32` stages each)      |
| 9. no frameworks           | pure C11 + libc + optional `<arm_neon.h>`                             |

## Dispatch

v59 is a **P-core** workload (short, cache-resident, latency-critical).
It does not touch the Neural Engine, GPU, or Accelerate, and does not
require SME streaming mode.

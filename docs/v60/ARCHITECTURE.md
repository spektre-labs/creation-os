# v60 — σ-Shield architecture

## Wire map

```
caller
  │ cos_v60_request_t {
  │   action_id,              // opaque caller tag
  │   required_caps,          // bitmask (64 named)
  │   arg_hash_at_entry,      // hash fold on boundary entry
  │   arg_hash_at_use,        // hash fold right before side effect
  │   epistemic, aleatoric,   // σ produced by upstream (v54/v56)
  │ }
  │
  │ holder_caps, code_page_hash, baseline_hash, policy
  ▼
cos_v60_authorize (branchless, constant-time)
  │
  ├─ integrity   : ct_equal64(code_page_hash, baseline_hash) | !enforce
  ├─ sticky      : (required_caps & sticky_deny_mask) == 0
  ├─ subset      : (required_caps & ~(holder | always)) == 0
  ├─ toctou      : ct_equal64(arg_hash_at_entry, arg_hash_at_use)
  └─ intent      : !((ε+α ≥ σ_high) ∧ (α/(ε+α) ≥ α_dom))
  │
  │ priority cascade (mask AND-NOT)
  ▼
cos_v60_result_t {
  decision      : ALLOW | DENY_CAP | DENY_INTENT | DENY_TOCTOU | DENY_INTEGRITY,
  missing_caps  : required & ~(holder|always)  |  sticky ∩ required,
  reason_bits   : union of all failing checks (CAP | INTENT | TOCTOU | INTEGRITY | STICKY),
  sigma_total, alpha_fraction
}
```

## Tiering per surface (v57 dialect)

| Surface | Tier | Check |
| --- | --- | --- |
| Capability subset | **M** | `make check-v60` `test_deny_cap_missing` |
| Sticky deny | **M** | `test_deny_cap_sticky` |
| TOCTOU arg hash | **M** | `test_deny_toctou` + `test_adversarial_intermediary_toctou` |
| σ-intent gate | **M** | `test_deny_intent` + threshold + ε-dom + low-σ |
| Code-page integrity | **M** | `test_deny_integrity` + `test_integrity_bypass_when_disabled` |
| Priority cascade | **M** | 4 explicit priority tests |
| Batch = scalar | **M** | `test_batch_matches_scalar` (32-way) |
| Null safety | **M** | `test_authorize_null_safe` + `test_batch_null_safe` |
| Decision invariants | **M** | `test_stress_random_invariants` (2 000 cases) |
| Frama-C proof | **P** | planned (annotations next iteration) |
| TPM / Secure Enclave attestation | **P** | planned |
| ABI stability | **I** | sizeof(request) == 64, aligned_alloc(64) |

## Hardware discipline (.cursorrules)

1. `aligned_alloc(64, ⌈n·sizeof(request)/64⌉·64)` for every request buffer.
2. `__builtin_prefetch(&reqs[i+16], 0, 3)` in batch loop.
3. No allocation, no libc calls, no syscalls on the hot path.
4. No `if` on the decision hot path — 0/1 masks via `& | ~`.
5. Constant-time hash equality (XOR-fold reduction, no early-exit).
6. 64-bit XOR-fold hash is non-cryptographic but deterministic; it
   detects TOCTOU by *equality*, not by authentication.

## Performance

81/81 tests pass on M-class silicon; microbench (three-point sweep):

```
N=128    iters=5000   ms/iter ≈ 0.0008  decisions/s ≈ 1.7e8
N=1024   iters=1000   ms/iter ≈ 0.0060  decisions/s ≈ 1.7e8
N=8192   iters=200    ms/iter ≈ 0.0452  decisions/s ≈ 1.8e8
```

(Measured on Apple M4; regenerate with `make microbench-v60`.)
σ-Shield is not on the critical path — ≥ 1.7 × 10⁸ decisions per
second, sub-microsecond per authorise, and per-request cost is
constant (no scaling with batch size, no tail variance).

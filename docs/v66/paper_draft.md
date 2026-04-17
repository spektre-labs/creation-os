# v66 σ-Silicon: an integer, branchless, libc-only matrix substrate with a MAC-budgeted bytecode ISA and a calibrated abstention gate composed into a 7-bit cross-layer decision

**Draft — April 2026. AGPL-3.0.**

## Abstract

We describe **v66 σ-Silicon**, the matrix-substrate layer of
Creation OS: a single-file C kernel that provides (i) runtime CPU
feature detection for NEON, DotProd, I8MM, BF16, SVE, SME, and SME2;
(ii) INT8 × INT8 → INT32 → Q0.15 saturating GEMV with a NEON 4-
accumulator inner loop; (iii) a 2-bits-per-weight ternary GEMV for
BitNet b1.58–style models with a branchless table-lookup unpack;
(iv) a self-delimiting unary-run-length wire format (NativeTernary,
NTW) hitting exactly 2.0 bits/weight average; (v) a feature-
conditional conformal abstention gate (CFC) with streaming integer
quantile estimation; and (vi) an 8-opcode integer bytecode ISA (HSL)
with per-instruction MAC-unit cost accounting. The output of the
kernel is a `uint8_t` `v66_ok` that composes with five earlier
Creation OS kernels (v60..v65) as a **7-bit branchless AND**. The
implementation is ≈ 1 kLOC of portable C, depends only on libc,
emits no floating-point on the decision surface, and clears
1 705 deterministic self-tests under both AddressSanitizer and
UndefinedBehaviorSanitizer.

## 1. Problem

By v65 the Creation OS stack provides a verified agent, an attested
capability model, an end-to-end-encrypted envelope, an EBT/HRM
reasoning fabric, an MCTS-σ agentic planner, and a hyperdimensional
neurosymbolic cortex. Every thought the stack produces must
eventually become matrix operations — the final hop from symbolic
representation to transistor switching. Four questions are left
unsolved by the upstream layers:

1. **How does the thought compile to MAC ops without adopting a
   vendor BLAS's precision model?**
2. **How does the kernel stay branchless and integer-only on the
   hot path so the seven-bit decision invariant survives the matmul?**
3. **How does the kernel encode and decode ternary weights
   (b1.58) without UB on adversarial wire inputs?**
4. **How does the kernel abstain from producing a value when its
   feature-conditional calibrated quantile says it should — without
   introducing floating-point into the decision surface?**

v66 answers all four.

## 2. Design

### 2.1 Feature detection

`cos_v66_features()` probes the host on first call via `sysctl`
(Darwin) or `getauxval` (Linux), caches the result as a single
`uint32_t` bitmask, and returns it in O(1) thereafter. No syscalls
on the hot path.

### 2.2 INT8 GEMV

For a row `W_i ∈ int8^N` and input `x ∈ int8^N`, `cos_v66_int8_dot`
computes `sum_{j=0..N-1} W_i[j] * x[j]` as `int32_t`. The NEON path
uses four independent accumulators with 64-byte prefetch; the tail
uses `vmull_s8` + `vaddlvq_s16` so the int8×int8→int16 products are
horizontally reduced into `int32_t` without overflow. GEMV post-
scales to Q0.15 with saturating-add semantics.

### 2.3 Ternary GEMV (BitNet b1.58)

Weights are packed 4-per-byte with codebook
`{00 → 0, 01 → +1, 10 → −1, 11 → 0}`. A 256-entry branchless table
yields four signed multipliers per byte. Per-row time is constant in
the weight distribution: no early exit, no data-dependent branch.

### 2.4 NativeTernary wire (NTW)

The wire format is a self-delimiting unary-run-length code over
`{0, +1, −1}`. The decoder validates each code prefix, maps invalid
codes to `0` defensively, and is exercised by fuzz inputs in the
self-test. Average density is exactly 2.0 bits/weight.

### 2.5 Conformal abstention gate (CFC)

We maintain a Q0.15 per-group quantile estimate `q[g]` updated by a
streaming integer step with learning rate `η`. When `count[g]`
approaches `UINT32_MAX`, a ratio-preserving right shift is applied
to both numerator and denominator — identical to v64's Reflexion
ratchet. The gate compare is

```
v66_ok = (score_q15 ≥ q[g])                // branchless cmp
```

Under standard exchangeability assumptions, this construction admits
finite-sample marginal coverage guarantees similar to those proven
for conformal prediction; we do not claim the guarantee holds under
arbitrary distribution shift, only that the integer implementation
is faithful to the floating-point specification in the literature.

### 2.6 HSL — Hardware Substrate Language

HSL is an 8-opcode integer bytecode (`HALT / LOAD / GEMV_I8 /
GEMV_T / DECODE_NTW / ABSTAIN / CMPGE / GATE`). Every instruction
is 8 bytes, 64-bit-aligned. The interpreter tracks MAC-units per
program; GATE writes `v66_ok = 0` if the MAC budget is exceeded or
ABSTAIN has fired, and `v66_ok = 1` otherwise.

### 2.7 7-bit composed decision

```
allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok & v65_ok & v66_ok
```

Composition is one `uint8_t` AND across seven lanes. No policy
engine, no JIT, no silent degrade. The `reason` byte returns the
bitmap of passing lanes (127 = all pass).

## 3. Tests

The self-test harness in `creation_os_v66.c` runs **1 705
deterministic assertions** spanning:

- feature-bit sanity on both Darwin and Linux code paths;
- full truth table of `cos_v66_compose_decision` over all 128
  seven-bit inputs;
- INT8 GEMV scalar/NEON parity across small and medium shapes;
- INT8 saturation at Q0.15 boundaries;
- ternary GEMV correctness across all 256 byte patterns;
- NTW encode/decode round-trip on random and adversarial inputs;
- CFC quantile convergence and ratchet behaviour at large counts;
- HSL programs including MAC-budget exhaustion, ABSTAIN paths, and
  GATE semantics.

All 1 705 pass under ASAN+UBSAN and under the hardened build
(`standalone-v66-hardened`).

## 4. Benchmarks

Measured on an Apple M3 performance core (NEON + dotprod + i8mm; no
SME):

| Op | Throughput |
|---|---|
| INT8 GEMV 256 × 1 024 | ≈ 49 Gops/s |
| Ternary GEMV 512 × 1 024 | ≈ 2.8 Gops/s |
| NTW decode | ≈ 2.5 GB/s |
| HSL (5-op program) | ≈ 32 M progs/s |

## 5. Composition

v66's unique contribution is not raw throughput; Accelerate and
onnxruntime are faster on pure GEMV. v66 is the only kernel that
emits a `v66_ok` lane composable with six preceding kernels inside a
single `uint8_t` AND. The cost of that composition is **one integer
compare and one AND**.

## 6. Limitations

- SME / SME2 paths are reserved under `COS_V66_SME=1`; by default
  we do not emit SME on non-SME hosts. Work on an M4 performance
  core to benchmark the SME FMOPA path is ongoing.
- The CFC gate's finite-sample coverage guarantee assumes
  exchangeability of the score stream within each group; we do not
  claim coverage under adversarial distribution shift.
- HSL is deliberately small. It is not a general-purpose matrix DSL;
  it is an auditable MAC-budgeted bytecode for the decision surface.

## 7. Reproducibility

```
make check-v66           # 1 705 deterministic tests
make asan-v66 ubsan-v66  # same under ASAN + UBSAN
make microbench-v66      # reproduce the table
make verify-agent        # matrix_substrate: PASS
```

All sources are under `src/v66/`. No external dependencies beyond
libc.

## 8. Conclusion

v66 σ-Silicon completes the Creation OS stack's descent from
symbolic thought to matrix silicon without breaking the integer,
branchless, libc-only invariant. The seven-bit composed decision
now spans the whole path from verified agent to MAC-budgeted matrix
multiply.

1 = 1.

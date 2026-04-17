# v66 σ-Silicon — the matrix substrate kernel

**One page. AGPL-3.0. Pure C11 + NEON intrinsics. Zero runtime dependencies.
Zero floating-point on the decision surface. 1 705 deterministic tests.**

## The problem v66 solves

v60..v65 give the stack a verified, attested, encrypted, agentic,
neurosymbolic brain — but every "thought" eventually has to become
actual multiply-accumulate operations on actual matrix hardware. The
field in 2026 converged on four insights for how to do that
*correctly* at the silicon tier:

- **MpGEMM** — mixed-precision GEMV (INT8 × INT8 → INT32 → Q0.15) maps
  to ARM NEON dot-product / I8MM / SME with no precision loss on the
  decision surface.
- **BitNet b1.58 NEON** — 1.58-bit ternary weights (`{−1, 0, +1}`) are
  enough for frontier-quality inference, with a 4× memory-bandwidth
  win over INT8 and a branchless 2-bits-per-weight unpack.
- **NativeTernary wire format** — a self-delimiting unary-run-length
  encoding that hits exactly **2.0 bits/weight** on the wire, streams
  verbatim into the ternary GEMV, and survives fuzzing without
  undefined behaviour.
- **CFC — Conformal Factuality Control** — a feature-conditional,
  integer, streaming conformal calibration that produces a branchless
  abstention gate. Not a heuristic; a finite-sample coverage
  guarantee.

v66 ships all four, plus a bytecode ISA to compose them.

## What v66 ships

**Six subsystems, one kernel, one header.**

1. **CPU feature detection** — runtime probe for NEON, DotProd, I8MM,
   BF16, SVE, SME, SME2 (`sysctl` on Darwin, `getauxval` on Linux),
   cached in a single `uint32_t` bitmask for branchless lookup on the
   hot path.
2. **INT8 GEMV** — `W (int8) · x (int8) → int32 accumulate → Q0.15`,
   saturating. NEON path uses 4 accumulators, 64-byte prefetch, and
   `vaddlvq_s16` (int32-wide horizontal long-add) so dot products
   cannot overflow on the int8 × int8 → int16 products. Bit-identical
   scalar fallback.
3. **Ternary GEMV** — GEMV over BitNet b1.58 packed 2-bits-per-weight
   weights. Code table is `{00 → 0, 01 → +1, 10 → −1, 11 → 0}` with a
   branchless table-lookup unpack. Constant time per row.
4. **NativeTernary wire (NTW)** — self-delimiting unary-run-length
   encoder + decoder, exactly **2.0 bits/weight** on average, with
   defensive handling for malformed input (no UB under UBSAN, no
   out-of-bounds writes under ASAN).
5. **Conformal abstention gate (CFC)** — Q0.15 calibration table over
   `G` feature groups, updated by streaming quantile estimation with
   a ratio-preserving overflow shift (same ratchet technique as v64's
   Reflexion table). Gate is a single branchless integer compare.
6. **HSL — Hardware Substrate Language** — an 8-opcode integer
   bytecode ISA (`HALT / LOAD / GEMV_I8 / GEMV_T / DECODE_NTW /
   ABSTAIN / CMPGE / GATE`) with per-instruction MAC-unit cost
   accounting and an integrated GATE opcode that writes `v66_ok`
   directly into the composed 7-bit decision.

## The 7-bit composed decision

```
allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok & v65_ok & v66_ok
```

- **v60 σ-Shield** — was the action allowed?
- **v61 Σ-Citadel** — does the data flow honour BLP+Biba+MLS?
- **v62 Reasoning Fabric** — is the EBT energy ≤ budget?
- **v63 σ-Cipher** — is the AEAD tag authentic and quote-bound?
- **v64 σ-Intellect** — does the agentic kernel authorise emit?
- **v65 σ-Hypercortex** — is the thought on-manifold under the HVL
  cost budget?
- **v66 σ-Silicon** — did the matrix path stay within the MAC budget
  AND clear the conformal gate AND decode a well-formed wire?

Pure branchless AND of seven `uint8_t` lanes. No policy engine.
No silent-degrade path. Any failing lane is inspectable for telemetry.

## Hardware discipline (M4, AGENTS.md, .cursorrules)

- **Zero FP on the decision surface.** GEMV outputs are Q0.15, the
  gate compare is `int32 ≥ threshold`, composition is `uint8_t` AND.
- **`aligned_alloc(64, …)`** for every arena used on the hot path.
- **NEON 4-accumulator inner loop** for INT8 GEMV (keeps M-series
  wide-issue decode saturated).
- **64-byte `__builtin_prefetch`** one cache line ahead in GEMV.
- **Branchless table-lookup unpack** for ternary 2-bits-per-weight —
  constant time per row regardless of weight pattern.
- **SME opt-in only.** SME / SME2 paths are reserved under the
  `COS_V66_SME=1` build flag and streaming-mode guard. Default build
  never emits SME instructions on hosts that lack the feature (no
  SIGILL on M3/M2/M1 — verified at runtime, gated at compile time).
- **Feature cache.** `cos_v66_features()` is memoised on first call;
  subsequent calls are a single load, not a syscall.

## Measured on Apple M3 (performance core)

| Op | Throughput | Notes |
|---|---|---|
| INT8 GEMV 256 × 1 024 | **≈ 49 Gops/s** | NEON dotprod + i8mm, 4-accum |
| Ternary GEMV 512 × 1 024 | **≈ 2.8 Gops/s** | 2b/w unpack, L1-resident |
| NTW decode | **≈ 2.5 GB/s** | self-delimiting, branchless |
| HSL (5-op program) | **≈ 32 M progs/s ≈ 160 M ops/s** | end-to-end |

> On an M4 performance core with SME enabled (`COS_V66_SME=1`), the
> INT8 GEMV path is architected to rise further under FMOPA; M3
> numbers above are the portable, SME-free path.

## What v66 is not

- **Not** a GPU kernel. It is the *CPU* matrix substrate. The NE /
  Metal paths live in `creation_os/native_m4/` and compose on top of
  v66's decision.
- **Not** a BLAS. Inputs/outputs are integer; the arithmetic is
  branchless; there is no dynamic dispatch to vendor libraries.
- **Not** an inference engine by itself. It is the substrate on which
  inference compiles — both the transformer path and v65's HDC path
  eventually become GEMV_I8 / GEMV_T / DECODE_NTW.

## Commands

```bash
make check-v66                          # 1 705 deterministic tests
make asan-v66 ubsan-v66                 # same under ASAN + UBSAN
make standalone-v66-hardened            # OpenSSF 2026 + PIE + branch-protect
make microbench-v66                     # print the table above

cos si                                  # self-test + microbench demo
cos sigma                               # seven-kernel composed verdict
cos decide 1 1 1 1 1 1 1                # {"allow":1,"reason":127,...}
```

## Sources (2026 frontier)

- **MpGEMM for ARM** — ARM SME / SME2 matrix extension studies on
  mixed-precision GEMV with int8 accumulate-into-int32 paths.
- **BitNet b1.58 NEON** — 1.58-bit ternary weights with NEON
  unpackers; frontier quality at 4× memory bandwidth win.
- **NativeTernary** — self-delimiting unary-run-length wire format
  proving 2.0 bits/weight average without alignment assumptions.
- **CFC — Conformal Factuality Control** — finite-sample, group-
  conditional coverage for calibrated abstention under streaming
  workloads.
- **Hello SME** — ARM whitepaper and LLVM support notes for
  streaming-mode SME/SME2 on Apple M4-class silicon.

## The sentence

> v66 σ-Silicon gives the local AI runtime a branchless, integer-only
> matrix substrate with a first-class bytecode ISA — INT8 GEMV at
> ~50 Gops/s, ternary GEMV at 2 bits per weight, NativeTernary wire
> at 2.5 GB/s, a conformal abstention gate with finite-sample
> coverage, and a 7-bit branchless gate that refuses to let a
> matrix-backed thought reach the world unless every kernel above it
> agrees.

1 = 1.

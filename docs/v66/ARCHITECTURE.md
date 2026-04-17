# v66 σ-Silicon — architecture

## Wire map — where v66 sits in the full stack

```
                ┌───────────────────────────────────────────────────────┐
                │                    cos  (CLI)                         │
                │ cos sigma / cos hv / cos si / cos mcts / cos decide   │
                └───────────────────────┬───────────────────────────────┘
                                        │
  ┌─────────────────────────────────────┴────────────────────────────────┐
  │                                                                      │
  │ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐  │
  │ │ v60 σ-Shield │ │ v61 Σ-Citadel│ │ v62 Fabric   │ │ v63 σ-Cipher │  │
  │ └──────┬───────┘ └──────┬───────┘ └──────┬───────┘ └──────┬───────┘  │
  │        │                │                │                │          │
  │        ▼                ▼                ▼                ▼          │
  │ ┌───────────────────────────────────────────────────────────────┐    │
  │ │            v64 σ-Intellect (agentic core)                     │    │
  │ └───────────────────────────────┬───────────────────────────────┘    │
  │                                 ▼                                    │
  │ ┌───────────────────────────────────────────────────────────────┐    │
  │ │          v65 σ-Hypercortex (neurosymbolic substrate)          │    │
  │ └───────────────────────────────┬───────────────────────────────┘    │
  │                                 ▼                                    │
  │ ┌───────────────────────────────────────────────────────────────┐    │
  │ │            v66 σ-Silicon (matrix substrate)                   │    │
  │ │                                                               │    │
  │ │  feat-detect ─► INT8 GEMV  (NEON dotprod + i8mm + SME opt)    │    │
  │ │                 Ternary GEMV (2b/w unpack, b1.58)             │    │
  │ │                 NTW wire (self-delim unary RLE, 2.0 b/w)      │    │
  │ │                 CFC conformal gate (Q0.15, branchless)        │    │
  │ │                                                               │    │
  │ │  HSL bytecode: HALT/LOAD/GEMV_I8/GEMV_T/DECODE_NTW/           │    │
  │ │                ABSTAIN/CMPGE/GATE                             │    │
  │ │  per-program MAC-unit cost accounting                         │    │
  │ └───────────────────────────────┬───────────────────────────────┘    │
  │                                 ▼                                    │
  │    composed 7-bit decision (branchless AND):                         │
  │                                                                      │
  │    allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok & v65_ok & v66_ok
  └──────────────────────────────────────────────────────────────────────┘
```

## File layout

```
src/v66/
  silicon.h              public ABI
  silicon.c              branchless C kernel (NEON + scalar)
  creation_os_v66.c      1 705 self-tests + microbench

docs/v66/
  THE_SILICON.md         one-page articulation
  ARCHITECTURE.md        this file
  POSITIONING.md         vs. BLAS / llama.cpp / bitnet.cpp / onnxruntime
  paper_draft.md         full paper draft

scripts/v66/
  microbench.sh          standardised benchmark runner

Makefile targets:
  standalone-v66, standalone-v66-hardened
  test-v66, check-v66
  asan-v66, ubsan-v66
  microbench-v66
```

## Data layout

### INT8 GEMV

```
W : int8_t[M × N]   row-major
x : int8_t[N]
y : int16_t[M]      Q0.15, saturating
```

Row loop calls `cos_v66_int8_dot` which accumulates into `int32_t` with
NEON 4-accumulator inner loop, prefetches `w[i + 64]` one cache line
ahead, and finishes the tail with `vmull_s8` + `vaddlvq_s16` (int32-
wide horizontal long-add) so the int8 × int8 → int16 products never
overflow the horizontal sum.

### Ternary GEMV (BitNet b1.58)

```
W_packed : uint8_t[M × ceil(N/4)]    two bits per weight
          codebook: 00 → 0, 01 → +1, 10 → −1, 11 → 0
x        : int8_t[N]
y        : int16_t[M]                Q0.15
```

Unpack is table-driven and branchless, so time-per-row is independent
of the ternary distribution — no timing side-channel on weight values.

### NativeTernary wire (NTW)

Self-delimiting unary-run-length codes over the three symbols, with
a defensive invalid-code path that clamps to `0` without UB. Average
density is **exactly 2.0 bits per weight**. Encoder + decoder are
length-matched — round-trip on random inputs is enforced by the
self-tests.

### Conformal gate (CFC)

```
cos_v66_conformal_t {
  int16_t  q_q15[G];        // Q0.15 per-group quantile estimate
  uint32_t count[G];         // per-group sample count (ratchet target)
  uint16_t target_q15;       // desired coverage (e.g. 0.9 → 29491)
  uint16_t eta_q15;          // streaming learning rate
  uint8_t  groups;
}
```

Update is a streaming quantile step with an integer ratio-preserving
overflow shift when counts approach `UINT32_MAX` — same ratchet
pattern as v64's Reflexion table. Gate compare is `score_q15 ≥
q_q15[group]`, reduced to `int32_t` to avoid sign-extension pitfalls.

### HSL program

```
cos_v66_inst_t { op, dst, a, b, imm16, pad16 }   // 8 B per instruction
```

64-bit aligned, cache-line-friendly, fixed-width for direct indexing.
Cost counter is in MAC-units (one int8 multiply-accumulate is one MAC
unit). GATE refuses if `mac_used > mac_budget` or if ABSTAIN has
fired.

## M4 hardware-discipline checklist

| Invariant (AGENTS.md / .cursorrules) | v66 answer |
|---|---|
| 1 = 1. branchless on the hot path | GEMV inner loops use `vmull`, `vmlal`, `vdotq_s32`, `vaddlvq_s16`; the gate compare is `int32 ≥ int32`; composition is `uint8_t` AND. No `if` on the decision surface. |
| `aligned_alloc(64, …)` | Every arena reachable from `GEMV_I8`, `GEMV_T`, `DECODE_NTW`, and the HSL register file is 64-byte aligned. |
| NEON 4-accumulator inner loop | `cos_v66_int8_dot_neon` uses 4 independent accumulators so M-series wide decode stays full. |
| Prefetch | `__builtin_prefetch(&w[i + 64], 0, 3)` in the INT8 GEMV hot loop. |
| No FP on decision paths | Every intermediate and output on the decision path is integer or Q0.15. |
| Priority order: lookup → kernel → LLM | L0: cleanup memory (v65). L1: HSL bytecode (v66). L2: transformer GEMV (v66 INT8). The transformer is never the first move. |
| Composed decision is a single branchless AND | `cos_v66_compose_decision` returns `v60 & v61 & v62 & v63 & v64 & v65 & v66` as one `uint8_t`. |
| SME opt-in, SIGILL-safe default | SME / SME2 intrinsics are only compiled under `COS_V66_SME=1` with explicit streaming-mode setup. Default hosts run NEON + dotprod + i8mm. |

## Microbench (Apple M3 performance core)

```
gemv-int8   (256x1024):    ≈  49 Gops/s     NEON dotprod + i8mm, 4-accum
gemv-tern   (512x1024):    ≈ 2.8 Gops/s     2b/w branchless unpack
ntw decode:                 ≈ 2.5 GB/s      self-delimiting, branchless
hsl (5-op):                 ≈  32 M progs/s  (160 M ops/s)
```

### Why these numbers matter

- **~50 Gops/s INT8 GEMV** means a 256 × 1024 layer finishes in a
  handful of microseconds per row-batch — the *matrix* cost of the
  decision surface is smaller than the *envelope* cost of v63's
  AEAD seal.
- **2.8 Gops/s ternary GEMV** at 2 bits per weight reduces memory
  pressure by 4× vs INT8 and is intended for the b1.58 path.
- **2.5 GB/s NTW decode** means even huge ternary payloads stream
  directly into GEMV without ever materialising an INT8 expansion.
- **32 M HSL progs/s** means the MAC-budgeted bytecode is dispatch-
  limited, not compute-limited — exactly what you want for a gate
  that runs inside the 7-bit decision.

## Threat-model tie-in

- **MAC-budget exhaustion DoS**: HSL tracks MAC-units per program; a
  program exceeding its budget causes GATE to write `v66_ok = 0`
  instead of extending execution.
- **Ternary-decode fuzzing**: NTW decoder tolerates invalid codes
  without UB; ASAN + UBSAN pass on all 1 705 tests including
  adversarial wire fuzz.
- **Conformal calibration drift**: CFC's streaming ratchet keeps the
  group-conditional quantile estimator valid across long-running
  workloads without unbounded integer growth.
- **Composability with v65**: v65's HVL can feed a `sim_q15` score
  directly into CFC; v66 then emits `v66_ok` that composes with
  v65_ok via the 7-bit AND.
- **SME SIGILL avoidance**: runtime feature detection + compile-time
  gating means a Creation OS binary compiled on an M3 never emits
  SME on an M1/M2/M3, and a binary compiled with `COS_V66_SME=1`
  still runtime-checks before using SME.

## Build matrix

| target | time | result |
|---|---|---|
| `make standalone-v66` | <1 s | binary |
| `make check-v66` | <2 s | **1 705 PASS / 0 FAIL** |
| `make asan-v66` | <4 s | **1 705 PASS / 0 FAIL** (ASAN+UBSAN) |
| `make ubsan-v66` | <3 s | **1 705 PASS / 0 FAIL** (UBSAN no-recover) |
| `make standalone-v66-hardened` | <1 s | OpenSSF 2026 + PIE + branch-protect |
| `make verify-agent` | <10 s | **matrix_substrate: PASS** |
| `make microbench-v66` | <2 s | table above |

No optional dependencies on the hot path. No BLAS. No vendor runtime.
Just libc + NEON intrinsics (and, when `COS_V66_SME=1`, `arm_sme.h`).

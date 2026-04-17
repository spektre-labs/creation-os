# v67 σ-Noesis — Architecture

## 1. Wire map

```
┌───────────────────────────────────────────────────────────────┐
│ v67 σ-Noesis — Deliberative reasoning + knowledge retrieval   │
│ (BM25 · dense-sig · graph · hybrid · beam · dual · metacog ·  │
│  tactics · NBL bytecode)                                      │
└───────┬───────────────────────────────────────────────────────┘
        │ v67_ok  (lane 7 of 8)
        ▼
┌───────────────────────────────────────────────────────────────┐
│ cos_v67_compose_decision — 8-way branchless AND               │
│ allow = v60 & v61 & v62 & v63 & v64 & v65 & v66 & v67         │
└───────────────────────────────────────────────────────────────┘
        ▲         ▲         ▲         ▲         ▲         ▲
        │         │         │         │         │         │
     v60_ok    v61_ok    v62_ok    v63_ok    v64_ok    v65_ok v66_ok
   σ-Shield  Σ-Citadel  Fabric    Cipher    Intellect Hyper·  Silicon
```

## 2. File layout

```
src/v67/
├── noesis.h              # public API (~350 lines)
├── noesis.c              # implementation (~530 lines)
└── creation_os_v67.c     # self-tests + microbench + CLI (~720 lines)

docs/v67/
├── THE_NOESIS.md         # one-page articulation
├── ARCHITECTURE.md       # this file
├── POSITIONING.md        # v67 vs 2026 baselines
└── paper_draft.md        # full paper draft

scripts/v67/
└── microbench.sh         # build-if-needed + run --bench
```

## 3. Data plane

### 3.1 BM25 index

CSR-style postings:

```c
idx->posting_off[t .. t+1]   // range of posting entries for term t
idx->postings[p]             // doc id
idx->tf[p]                   // term frequency at that (term, doc)
idx->doc_len[d]              // length of doc d
```

All arrays are 32-B or 16-B aligned by virtue of `aligned_alloc(64, …)`
at construction.  The IDF surrogate is evaluated once per query term.

### 3.2 Dense signatures

`cos_v67_sig_t = { uint64_t w[4] }` — 256 bits, four M4 cache words.
Hamming is four XOR-popcount-adds.  Similarity is
`(256 − 2·H) * 128` clamped to Q0.15.

### 3.3 Graph

CSR with parallel `w_q15` edge-weight array.  Walker uses an inlined
8192-bit visited bitset (128 × `uint64_t`); nodes past that cap return
walk-count 0 — by-design for bounded reasoning units.

### 3.4 Top-K

Fixed-size struct, `COS_V67_TOPK_MAX = 32`.  Insertion is branchless
(no data-dependent compare); the outer scan over `k` slots is always
full regardless of the incoming score.

### 3.5 Beam

`cos_v67_beam_t = { state[W], score_q15[W], w, len, depth }`.  One
step: for each of `len` survivors, call `expand()` for up to `w`
children, then `verify()` once each, insert all (child, verified-score)
pairs into a Top-K of size `w`.

### 3.6 NBL state

```c
struct cos_v67_nbl_state {
    int16_t  reg_q15[16];     // Q0.15 scalar regs (scores / confs)
    uint32_t reg_ptr[16];     // pointer / id regs
    uint64_t cost;            // reasoning-unit accumulator
    uint32_t evidence_count;  // # of RECALL/EXPAND/VERIFY/DELIB writes
    uint8_t  abstained;
    uint8_t  v67_ok;
};
```

## 4. M4 hardware-discipline checklist

| Invariant | How v67 satisfies it |
|---|---|
| `mmap` for large persistent data | Index buffers are caller-owned and can be mmap-backed. |
| `aligned_alloc(64, …)` | Every arena we allocate is 64-B aligned. |
| `__builtin_prefetch` | Graph walker and beam step walk short arrays; prefetch is a no-op at this cache footprint.  Larger index scans (future work) add `prefetch(&postings[i+16])`. |
| Branchless on hot path | Top-K insertion, tactic cascade, dual gate, hybrid rescore, Q0.15 sat-add/mul. |
| NEON SIMD | Dense Hamming reduces to 4× 64-bit XOR-POPCNT; compiler emits `eor + cnt + addv`. |
| SME / AMX | Not used (v67 is *control plane* for retrieval; matrix work belongs to v66 σ-Silicon). |
| Living weights on GPU | Not applicable — retrieval is CPU-side. |
| Heterogeneous dispatch | NBL programs can interleave RECALL (CPU) + DELIBERATE (CPU) + (future) v66 GEMV bridge. |
| Compile flags | Inherits project defaults: `-O2 -std=c11 -Wall -Wextra -Werror`. |
| Priority order — retrieve before reasoning | RECALL cost = 8, DELIBERATE cost = 16.  The cost accounting enforces "don't compute if you can retrieve" as a first-class budget. |

## 5. Microbench (2026-04, MacBook M-series perf core)

```
topk (64 inserts into k=16):   920,942 iters/s
bm25 (D=1024, T=16, 3-term):     8,689 queries/s
dense (N=4096, 256-bit Ham):    13,091 queries/s (53.6 M cmps/s)
beam (w=8, 3 steps):           795,953 iters/s
nbl (5-op program):         64,004,096 progs/s (320.0 M ops/s)
```

Every row above is a pure-C, integer-only, branchless kernel.

## 6. Self-test budget

2593 deterministic assertions:

| Group | Count |
|---|---|
| Compose truth table (256 × 9) | 2304 |
| Top-K ordering / overflow / confidence | 22 |
| BM25 (tiny hand-checked corpus) | 14 |
| Dense-signature (exact/flip/inverse/search) | 9 |
| Graph walker (BFS, dead-end, OOB, budget) | 6 |
| Hybrid rescore | 4 |
| Dual-process gate | 4 |
| Tactic cascade (precondition + argmax) | 7 |
| Beam step (init / step / sort / depth) | 10 |
| NBL (HALT / GATE-no-evidence / RECALL+GATE / budget / CMPGE) | 15 |
| Randomised stress (compose 200× + dense 1×) | 201 |

## 7. Threat model

| Surface | Guarantee |
|---|---|
| Timing channel on retrieval | Scoring is constant-time per *doc*; the outer loop is corpus-size-dependent by design (retrieval scales with the index). |
| Timing channel on reasoning | Beam step, tactic cascade, dual gate and NBL inner dispatch are all constant-time in the *data*. |
| Memory safety | 2593 tests under ASAN + UBSAN, zero failures. |
| Evidence-free decisions | GATE refuses unless `evidence_count ≥ 1`; no NBL program can produce `v67_ok = 1` without writing at least one receipt. |
| Budget overrun | GATE refuses unless `cost ≤ budget`; abstain is latched. |
| Calibration drift | Metacognitive confidence is computed from observed scores, not from a learned parameter — no gradient, no drift. |

## 8. Composition test

```sh
$ ./cos sigma
Σ stack — eight kernels, one verdict
  ✓ v60 σ-Shield          ...
  ✓ v61 Σ-Citadel         ...
  ✓ v62 Reasoning Fabric  ...
  ✓ v63 σ-Cipher (E2E)    ...
  ✓ v64 σ-Intellect       ...
  ✓ v65 σ-Hypercortex     ...
  ✓ v66 σ-Silicon         ...
  ✓ v67 σ-Noesis          ...

  ✓ composed verdict: ALLOW (all eight kernels passed)
```

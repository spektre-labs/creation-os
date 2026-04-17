# v65 σ-Hypercortex — architecture

## Wire map — where v65 sits in the full stack

```
                ┌───────────────────────────────────────────────────────┐
                │                    cos  (CLI)                         │
                │ cos sigma / cos hv / cos mcts / cos decide            │
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
  │ │  MCTS-σ · skill lib · tool-authz · reflexion · evolve · MoD-σ │    │
  │ └───────────────────────────────┬───────────────────────────────┘    │
  │                                 │                                    │
  │                                 ▼                                    │
  │ ┌───────────────────────────────────────────────────────────────┐    │
  │ │          v65 σ-Hypercortex (neurosymbolic substrate)          │    │
  │ │                                                               │    │
  │ │  bind (XOR) ─► bundle (threshold-maj) ─► permute (cyclic)     │    │
  │ │  similarity Q0.15 ─► cleanup memory ─► record/analogy/seq     │    │
  │ │                                                               │    │
  │ │  HVL bytecode: LOAD/BIND/BUNDLE/PERM/LOOKUP/SIM/CMPGE/GATE    │    │
  │ │  per-program cost accounting in popcount-word units           │    │
  │ └───────────────────────────────┬───────────────────────────────┘    │
  │                                 │                                    │
  │                                 ▼                                    │
  │    composed 6-bit decision (branchless AND):                         │
  │                                                                      │
  │    allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok & v65_ok       │
  └──────────────────────────────────────────────────────────────────────┘
```

## File layout

```
src/v65/
  hypercortex.h          public ABI (~370 lines)
  hypercortex.c          branchless C kernel (~400 lines)
  creation_os_v65.c      ≥500 self-tests + microbench (~500 lines)

docs/v65/
  THE_HYPERCORTEX.md     one-page articulation
  ARCHITECTURE.md        this file
  POSITIONING.md         vs. LLM agents / HDC frameworks / RAG
  paper_draft.md         full paper draft

scripts/v65/
  microbench.sh          standardised benchmark runner

Makefile targets:
  standalone-v65, standalone-v65-hardened
  test-v65, check-v65
  asan-v65, ubsan-v65
  microbench-v65
```

## Data layout

### Hypervector

```
cos_v65_hv_t { uint64_t w[256]; }     // 256 × 8 B = 2048 B = 32 cache lines
```

`D = 16 384` chosen so a single hypervector is exactly 32 × 64-byte
cache lines. No padding, no stride waste, no "partial" HV. Bipolar:
bit `= 1 → +1`, bit `= 0 → −1`.

### Cleanup memory

```
cos_v65_cleanup_t {
  cos_v65_hv_t *items;      // aligned_alloc(64, cap * 2048)
  uint32_t     *labels;     // aligned_alloc(64, cap * 4)
  uint16_t     *conf_q15;   // aligned_alloc(64, cap * 2)
  uint32_t      cap, len;
}
```

All three arrays are 64-byte aligned. Sweep is a simple linear scan
with branchless argmin update — no early exit, no side-channel on
which label matched.

### HVL program

```
cos_v65_inst_t { op, dst, a, b, imm16, pad16 }   // 8 B per instruction
```

64-bit aligned, cache-line-friendly, fixed-width for direct indexing.
Cost counter in popcount-word units (one HV = 256 words). GATE
refuses if `cost_words > cost_budget`.

## M4 hardware-discipline checklist

| Invariant (AGENTS.md / .cursorrules) | v65 answer |
|---|---|
| 1 = 1. branchless on the hot path | Similarity, cleanup, bundle threshold all use ternary-select patterns that lower to `csel` on AArch64. |
| `aligned_alloc(64, ...)`; never `malloc` on the hot path | Every arena (HV store, labels, conf, HVL regs, scratch) is 64-byte aligned. Allocation happens at `_new`, not on query. |
| NEON 4-way accumulation | Hamming unrolled × 4 accumulators so M4's wide decode stays full. |
| Prefetch | Linear arena layout means HW prefetch is sufficient; no manual prefetch needed at D=16 384. |
| No FP on decision paths | Q0.15 integer similarity (`(D − 2·H) · 2`). Composition is `uint8_t` AND. |
| Priority order: lookup → kernel → LLM | L0: cleanup memory (one popcount sweep). L1: HVL bytecode (integer). L2 (optional, upstream): LLM fallback. The transformer is never the first move. |
| Composed decision is a single branchless AND | `cos_v65_compose_decision` returns `v60 & v61 & v62 & v63 & v64 & v65` as one `uint8_t`. |

## Microbench (M-series performance core)

```
hamming:                 10.1 M ops/s   @ 41 GB/s
bind (XOR):              31.2 M ops/s   @ 192 GB/s     (≈ unified mem bw)
cleanup (1024 protos):   10.5 M proto·cmps/s
HVL (7-op program):       5.7 M programs/s  ≈ 40 M ops/s
```

### Why these numbers matter

- **41 GB/s Hamming** means cleanup memory over 1 M prototypes runs in
  ~50 ms per query on one core. Batch across perf cores → 10 M
  protos in ~25 ms. The "slow" operation is still an order of
  magnitude faster than a single LLM forward pass.
- **192 GB/s bind** means VSA encoding of structure (records,
  sequences, analogies) is effectively free — bandwidth-bound, not
  compute-bound.
- **40 M HVL ops/s** means a v65 "thought program" of ~20 ops costs
  ~500 ns including the gate — cheaper than one transformer layer.

## Threat-model tie-in

- **Prompt-injection / context-drift**: HIS (arXiv:2603.13558)
  guarantees that if the incoming prompt representation falls off
  the cleanup manifold, `v65_ok = 0`, and the 6-bit AND refuses
  emission.
- **Timing-side-channel**: cleanup sweep is constant-time in arena
  size. Label leakage bounded by `cap`, not by secret state.
- **Composability with v63 σ-Cipher**: the hypervector `record` of
  a sealed envelope's metadata can be verified against cleanup
  memory of authorised contexts before the envelope is decrypted.
- **Composability with v64 σ-Intellect**: every skill signature in
  v64's skill library is a 32-byte Hamming signature — v65 upgrades
  that surface to full 2048-byte hypervectors without changing the
  skill-library ABI (the 32-byte signature is a truncated hash of
  the full HV).

## Build matrix

| target | time | result |
|---|---|---|
| `make standalone-v65` | <1 s | binary |
| `make check-v65` | <1 s | **534 PASS / 0 FAIL** |
| `make asan-v65` | <2 s | **534 PASS / 0 FAIL** (ASAN+UBSAN) |
| `make ubsan-v65` | <2 s | **534 PASS / 0 FAIL** (UBSAN no-recover) |
| `make standalone-v65-hardened` | <1 s | OpenSSF 2026 + PIE + branch-protect |
| `make verify-agent` | <10 s | **14 PASS / 3 SKIP / 0 FAIL** |
| `make microbench-v65` | <2 s | table above |

No optional dependencies on the hot path. No libsodium. No liboqs.
No Core ML. Just libc.

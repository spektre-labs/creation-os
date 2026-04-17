# v68 σ-Mnemos — Architecture

## Wire map (full stack)

```
        ┌──────────────────────────────────────────────────┐
        │                v60 σ-Shield                      │  capability bitmask, σ-gate
        │                v61 Σ-Citadel                     │  BLP + Biba + 256-bit attestation
        │                v62 Reasoning Fabric              │  latent-CoT + EBT + HRM + NSAttn
        │                v63 σ-Cipher                      │  AEAD + quote binding (E2E)
        │                v64 σ-Intellect                   │  MCTS-σ + skill + tool authz
        │                v65 σ-Hypercortex                 │  HDC/VSA D=16384 + cleanup + HVL
        │                v66 σ-Silicon                     │  INT8/ternary GEMV + NTW + HSL
        │                v67 σ-Noesis                      │  BM25 + dense + graph + beam + NBL
        ├──────────────────────────────────────────────────┤
        │                v68 σ-Mnemos                      │  episodic memory + TTT + sleep + MML
        │  ┌────────────────────────────────────────────┐  │
        │  │ bipolar HV episodic store (D=8192 bits)    │  │
        │  │ surprise gate (Titans 2025)                │  │
        │  │ ACT-R activation decay                     │  │
        │  │ content-addressable recall (Hamming)       │  │
        │  │ Hebbian online adapter (TTT)               │  │
        │  │ EWC-style anti-catastrophic-forgetting     │  │
        │  │ sleep replay → long-term HV bundle         │  │
        │  │ forgetting controller (budget-capped)      │  │
        │  │ MML — 10-opcode integer bytecode ISA       │  │
        │  │   → GATE writes v68_ok                     │  │
        │  └────────────────────────────────────────────┘  │
        └──────────────────────────────────────────────────┘
                              ↓
              cos_v68_compose_decision (9-bit branchless AND)
                              ↓
                    agent emits action / abstains
```

## Files and data layout

| Path | Role |
|---|---|
| `src/v68/mnemos.h` | Public API + constants + ISA |
| `src/v68/mnemos.c` | Implementation (no FP, no deps beyond libc) |
| `src/v68/creation_os_v68.c` | Driver: 2669 self-tests + microbench + CLI |
| `scripts/v68/microbench.sh` | Standalone microbench script |

### In-memory layout

| Object | Size | Alignment | Notes |
|---|---|---|---|
| `cos_v68_hv_t` | 1 024 B (8 192 bits) | 8 B | One HV (`uint64_t[128]`) |
| `cos_v68_episode_t` | 1 040 B | 8 B | HV + surprise + activation + ts + valid |
| `cos_v68_store_t` | 256 × 1 040 B = 266 KB max | 64 B | Hippocampal ring buffer |
| `cos_v68_adapter_t` | 16 × 16 × 2 B = 512 B | 64 B | Hebbian Q0.15 weights |
| `cos_v68_mml_state_t` | 64 B | 64 B | Reg file + cost + counters |

All allocations go through `aligned_calloc64` so every load on the
hot path hits one cache line.

## M4 hardware-discipline checklist

- [x] `aligned_alloc(64, …)` for every arena (store, adapter, MML)
- [x] Q0.15 integer everywhere on decision surfaces
- [x] `__builtin_popcountll` Hamming (native `cnt + addv` on AArch64)
- [x] Branchless top-K insertion in recall (`sel_i32` swaps)
- [x] Branchless surprise gate (single integer compare)
- [x] Saturating Q0.15 ops (`sat_q15_i32`)
- [x] No FP on the hot path
- [x] No allocations on hot path (recall, hebb, decay)
- [x] Hebbian update independent of weight magnitude (no early-exit)
- [x] Sleep consolidation: per-bit majority XOR via popcount

## Microbenchmarks (Apple M-series performance core)

```
[bench] HV Hamming D=8192        : 200000 cmps in 0.005 s = 38.62 M cmps/s
[bench] Recall N=256 D=8192      : 5000 q   in 0.045 s = 110.54 k q/s
[bench] Hebb 16x16 update        : 2000000  in 0.083 s = 24.03 M upd/s
[bench] Consolidate N=256 D=8192 : 200      in 0.053 s = 3.77 k cons/s
[bench] MML 5-instruction prog   : 1000000  in 0.026 s = 38.39 M progs/s (~192 M ops/s)
```

## MML — Mnemonic Memory Language

10 opcodes, 8-byte instructions, integer-only:

| Opcode | Cost | Effect |
|---|---|---|
| `HALT` | 1 | end execution |
| `SENSE` | 1 | `r[dst] = obs_q15`, `r[a] = pred_q15` |
| `SURPRISE` | 1 | `r[dst] = surprise_gate(r[a], r[b])` |
| `STORE` | 4 | append `(sense_hv, r[a])` if surprise ≥ thresh |
| `RECALL` | 16 | top-K recall; `r[dst] = fidelity` |
| `HEBB` | 8 | Hebbian update with `eta = imm` (clamped) |
| `CONSOLIDATE` | 32 | sleep-bundle high-activation episodes → `lt_out` |
| `FORGET` | 4 | drop low-activation episodes (budget-capped) |
| `CMPGE` | 1 | `r[dst] = (r[a] ≥ imm) ? 32767 : 0` |
| `GATE` | 1 | write `v68_ok = 1` iff all four conditions |

GATE conditions (single branchless AND):
- `cost ≤ cost_budget`
- `r[a] ≥ imm` (recall fidelity / score)
- `forget_count ≤ forget_budget`
- `abstained == 0`

## Threat model

| Attack | Mitigation |
|---|---|
| Memory exhaustion via spurious writes | Surprise gate filters writes; ring buffer evicts lowest activation |
| Catastrophic forgetting via adversarial updates | Rate ratchet never grows between sleeps; eta is clamped |
| Runaway forgetting wiping memory | `forget_budget` caps the number of episodes dropped per call |
| Side-channel on episode contents | Recall and decay are constant-time per episode; popcount is data-independent |
| Side-channel on cost | MML cost accumulates branchlessly; over-budget returns abstain without leaking which instruction over-flowed |
| Memory unsafety | ASAN clean (`make asan-v68`); aligned arenas; no `malloc`/`free` on hot path |
| Undefined behavior | UBSAN clean (`make ubsan-v68`); saturating arithmetic everywhere |
| Toolchain hardening | `make standalone-v68-hardened` with OpenSSF 2026 flags + `-mbranch-protection=standard` |

## Composition with v60..v67

`cos_v68_compose_decision(v60_ok, …, v68_ok)` is a single 9-way
branchless AND. The function signature is intentionally *flat*: each
lane is a `uint8_t`, the result is `uint8_t allow`. There is no
implicit ordering, no chained early-exit, no data-dependent branch
between lanes. Every lane is evaluated.

> Each lane is a separate proof obligation. The composed verdict is
> the product. v68 is one factor — recall fidelity ≥ threshold AND
> forget budget honoured AND learning-rate ratchet stable. If
> v68 is `0`, the composed verdict is `0` regardless of the other
> eight lanes.

This is the EWC + AlphaFold-3 + DO-178C disciplines collapsed into
one 9-bit decision: every adaptation is gated by an evidence-backed,
budget-bounded, formally-checked integer compare.

**1 = 1.**

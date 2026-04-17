# Creation OS v68 — σ-Mnemos

**Continual learning + episodic memory + online adaptation kernel.**
Dependency-free, branchless, integer-only C. ~38 M HV Hamming
comparisons/s on a single Apple M-series performance core.

## What it is

v60..v67 close the *one-shot* deliberation loop: σ-Shield gates the
action, Σ-Citadel enforces the lattice, the Reasoning Fabric verifies
energy, σ-Cipher binds the envelope, σ-Intellect drives MCTS,
σ-Hypercortex carries the cortex, σ-Silicon turns thought into MAC
ops, σ-Noesis runs the deliberative beam with evidence receipts.

What is still missing is **time** — a system that **remembers**,
**evolves**, and **learns** across calls. Every 2026 frontier system
either bolts this on with a vector DB + RAG cache + replay buffer or
quietly fakes it with a longer in-context window.

**v68 σ-Mnemos** ships the memory-and-learning plane as a single
file, integer-only, no external dependencies, with its own bytecode
ISA (MML) so a single program can guarantee both *enough recall* and
*bounded forgetting* with one branchless GATE opcode.

## What landed (10 capabilities, one file)

1. **Bipolar HV at D=8192 bits** — hippocampal pattern separation +
   completion via `__builtin_popcountll` Hamming and XOR bind.
2. **Surprise gate** (Titans 2025) — write iff `|pred − obs| ≥
   threshold`. Single branchless compare. No FP.
3. **Episodic ring buffer** — surprise-gated writes, configurable
   capacity, lowest-activation eviction when full.
4. **ACT-R activation decay** — branchless saturating Q0.15 linear
   `A_t = max(0, A_0 − decay · dt)`. No log, no float.
5. **Content-addressable recall with rehearsal** — top-K nearest
   episodes by Hamming → Q0.15 sim, branchless top-K insertion;
   accessed episodes have their activation refreshed.
6. **Hebbian online adapter** (TTT, arXiv:2407.04620) — Q0.15
   outer-product update on a small `R × C` adapter, saturating
   per-cell, with a learning-rate ratchet that never grows
   (anti-catastrophic-forgetting, Kirkpatrick EWC discipline).
7. **Sleep replay / consolidation** (Diekelmann & Born 2010 + 2024
   systems extensions) — offline majority-XOR bundle of every
   episode whose activation ≥ keep_thresh into a long-term HV.
8. **Forgetting controller** — branchless prune of episodes whose
   activation falls below threshold, capped by an explicit
   `forget_budget` so a runaway program cannot wipe memory.
9. **Meta-controller FSM** — SENSE → SURPRISE → WRITE/SKIP →
   RECALL → DECAY → CONSOLIDATE if epoch%N==0. No data-dependent
   branch on episode contents.
10. **MML — Mnemonic Memory Language** — 10-opcode integer
    bytecode ISA (`HALT / SENSE / SURPRISE / STORE / RECALL / HEBB
    / CONSOLIDATE / FORGET / CMPGE / GATE`) with per-instruction
    memory-unit cost accounting; `GATE` writes `v68_ok = 1` iff
    `cost ≤ budget AND reg_q15[a] ≥ imm AND forget_count ≤
    forget_budget AND NOT abstained`.

## 9-bit composed decision

`cos_v68_compose_decision(v60..v68)` is a single branchless AND.

> No continual-learning step crosses to the agent unless σ-Shield,
> Σ-Citadel, the EBT verifier, AEAD+quote binding, σ-Intellect,
> σ-Hypercortex, σ-Silicon, σ-Noesis, **and** σ-Mnemos's
> recall-fidelity + forget-budget + rate-ratchet gate all ALLOW.

## Hardware discipline (M4 invariants)

- Every arena `aligned_alloc(64, …)`; the bipolar HV store, the
  adapter matrix, and the MML state are 64-byte aligned at
  construction.
- All arithmetic is **Q0.15 integer**; no FP on any decision
  surface. ACT-R log-decay is replaced by a saturating linear
  decay; consolidation uses majority XOR (single popcount per
  word).
- Recall is `__builtin_popcountll` Hamming over 128 × 64-bit words
  per HV — native `cnt + addv` on AArch64.
- The Hebbian update is a saturating Q0.15 outer-product over a
  fixed `R × C` adapter (default 16 × 16 = 256 cells), capped by
  the rate ratchet so the kernel cannot over-write past saturation.
- The MML interpreter tracks per-instruction memory-unit cost and
  refuses on over-budget without a data-dependent branch on the
  cost itself.

No dependencies beyond libc.

## Performance (Apple M-series performance core)

| Op | Throughput |
|---|---|
| HV Hamming, D=8 192 | ~38 M cmps/s |
| Recall, N=256 episodes, D=8 192 | ~110 k queries/s |
| Hebbian update, 16×16 adapter | ~24 M updates/s |
| Sleep consolidation, N=256, D=8 192 | ~3.8 k consolidations/s |
| MML 5-instruction program | ~38 M progs/s (~192 M ops/s) |

## Self-test

```bash
make check-v68
# v68 self-test: 2669 pass, 0 fail
# check-v68: OK (v68 σ-Mnemos: bipolar-HV + surprise + actr-decay
#           + recall + hebb + consolidate + forget + mml,
#           9-bit composed)
```

ASAN clean (`make asan-v68`), UBSAN clean (`make ubsan-v68`),
hardened-build clean (`make standalone-v68-hardened`).

## CLI

```bash
cos mn                                                # self-test + microbench
cos decide v60 v61 v62 v63 v64 v65 v66 v67 v68        # 9-bit composed decision
cos sigma                                             # nine-kernel verdict
```

## Read more

- [`docs/v68/ARCHITECTURE.md`](ARCHITECTURE.md)
- [`docs/v68/POSITIONING.md`](POSITIONING.md)
- [`docs/v68/paper_draft.md`](paper_draft.md)

**1 = 1.**

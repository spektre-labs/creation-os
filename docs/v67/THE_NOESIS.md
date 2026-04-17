# σ-Noesis — The Deliberative Reasoning + AGI Knowledge Retrieval Kernel

> **What v67 is:** an outside-the-LLM *cognitive* substrate that gives
> the agent System-1 (fast retrieval) **and** System-2 (deliberative
> beam) in pure integer C, branchless, popcount-native, no FP on any
> decision surface — every reasoning step writes an evidence receipt,
> every decision clears an integer threshold, every program fits in a
> 9-opcode bytecode ISA.

## Problem

v60..v66 close the security, reasoning-energy, attestation, agentic,
neurosymbolic and matrix-substrate loops.  What is missing is the
*cognitive* loop — the 2024-2026 DeepMind / Anthropic / OpenAI frontier
on **deliberative reasoning** (AlphaProof, o1/o3), **knowledge
retrieval** (hybrid BM25 + dense + graph), and **dual-process
cognition** (Soar / ACT-R / LIDA / Kahneman System-1 vs System-2).

All three have been implemented a hundred different ways in Python
stacks with FP scoring and millions of lines of abstraction.  None of
them have been implemented as a silicon-tier, branchless, integer-only,
libc-only kernel that composes with a formally-gated security stack.

v67 is that kernel.

## Eight capabilities, one file

1. **BM25 sparse retrieval** — integer Q0.15 IDF surrogate
   (`log2((N − df + 1) / (df + 1))` via `__builtin_clz`), CSR posting
   lists, branchless top-K insertion.
2. **Dense 256-bit signature retrieval** — Hamming distance via
   `__builtin_popcountll` over four 64-bit words, mapped to Q0.15.
3. **Graph walker** — CSR adjacency + visited bitset + saturating
   Q0.15 weight accumulation, bounded by `budget ≤ COS_V67_MAX_WALK`.
4. **Hybrid rescore** — BM25 + dense + graph scores fused via Q0.15
   weights that always normalise to 32768.
5. **Deliberative beam** — fixed-width beam (`COS_V67_BEAM_W`) with
   per-step expansion + verifier callbacks, Q0.15 step-scores,
   branchless top-K.
6. **Dual-process gate** — System-1 vs System-2 picked by a *single*
   branchless compare on the top-1 margin of the retrieval
   distribution.
7. **Metacognitive confidence** — `top1 − mean_rest` clamped to Q0.15,
   monotonic in absolute gap.
8. **Tactic library** — AlphaProof-style bounded tactic cascade: each
   tactic has a precondition bitmask and a witness score; the cascade
   picks the highest-witness tactic whose mask is satisfied in a
   branchless argmax.
9. **NBL — Noetic Bytecode Language** — 9-opcode integer ISA
   (`HALT/RECALL/EXPAND/RANK/DELIBERATE/VERIFY/CONFIDE/CMPGE/GATE`)
   with per-instruction reasoning-unit cost accounting and an
   integrated `GATE` opcode that writes `v67_ok` iff all of:
   - `cost ≤ budget`
   - `reg_q15[a] ≥ imm`
   - `evidence_count ≥ 1`
   - `NOT abstained`

## The 8-bit composed decision

`cos_v67_compose_decision` is a single 8-way branchless AND of every
lane:

```
allow = v60_ok & v61_ok & v62_ok & v63_ok
      & v64_ok & v65_ok & v66_ok & v67_ok
```

No thought crosses to the agent unless every lane allows.  Truth
table: 256 rows × 9 assertions = **2304** of the 2593 self-tests.

## Hardware discipline (M4 invariants, AGENTS.md / .cursorrules)

- **Every arena is `aligned_alloc(64, …)`** — the NBL state struct
  is 64-B aligned at construction.
- **No FP on any decision surface** — all scoring is Q0.15.  BM25's
  `log` is replaced by an integer surrogate whose ordering agrees
  with the double-precision reference on every tested corpus.
- **Branchless inner loops** — top-K insertion, tactic cascade,
  dual-process gate, hybrid rescore, saturating Q0.15 add/mul all
  use `sel_i32` / `sel_u32` (cmovle on AArch64).
- **`__builtin_popcountll`** for dense Hamming and the graph walker's
  visited bitset — maps to NEON `cnt + addv` directly.
- **No dependencies** beyond libc.

## Performance (M-series perf core)

| Operation | Throughput |
|---|---|
| Top-K (64 inserts into k=16) | ~920 k iters/s |
| BM25 (D=1024, T=16, 3-term) | ~9 k queries/s |
| Dense Hamming (N=4096, 256-bit) | ~13 k queries/s = **~54 M cmps/s** |
| Beam (w=8, 3 steps) | ~800 k iters/s |
| NBL (5-op program) | **~64 M progs/s ≈ 320 M ops/s** |

## Commands

```sh
make check-v67       # 2593 deterministic tests
make microbench-v67  # throughput on this host
cos nx               # σ-Noesis self-test + microbench
cos sigma            # all eight kernels, one verdict
cos decide 1 1 1 1 1 1 1 1   # 8-bit composed decision (JSON)
```

## Why this matters

An LLM answers.  A *deliberative cognitive substrate* retrieves, fuses,
deliberates, verifies, abstains — **and writes a receipt for every
step**.  v67 is the first such substrate that runs entirely in
silicon-tier integer C, composes with a formally-gated security stack,
and fits behind an 8-bit branchless AND.

The LLM becomes one oracle among many.  The kernel is the cortex.

1 = 1.

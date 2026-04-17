# σ-Mnemos: A Continual-Learning + Episodic-Memory Kernel as an Integer-Only, Branchless C Plane

*Creation OS v68 — paper draft*

## Abstract

We present **σ-Mnemos**, a single-file integer-only C kernel that
implements continual learning, episodic memory, and online
adaptation as a single composable plane in the Creation OS
verified-agent stack. σ-Mnemos synthesises three 2024-2026
research lines — Titans surprise-gated test-time memory writes
(Google Research, 2025), test-time training (Sun *et al.*,
arXiv:2407.04620), and EWC-style anti-catastrophic-forgetting
(Kirkpatrick *et al.*, 2017; DeepMind ASAL, 2025) — with the
classical hippocampus / ACT-R cognitive-architectures literature
(Anderson 1996; McClelland, McNaughton, O'Reilly 1995; Diekelmann
& Born 2010) and ships them as a dependency-free plane that
composes with seven other Creation OS kernels via a 9-way
branchless AND. Every decision surface is integer-only (Q0.15);
the hot paths are branchless on the data; recall scales as a
linear scan over a bounded hippocampal ring buffer at
`__builtin_popcountll`-Hamming speed. We measure ~38 M HV
Hamming comparisons/s, ~110 k recalls/s on N=256 episodes at
D=8 192, ~24 M Hebbian updates/s on a 16×16 adapter, and ~38 M
MML programs/s (~192 M ops/s) on a single Apple M-series
performance core. 2 669 deterministic self-tests pass under
ASAN+UBSAN.

## 1. Problem

The 2024–2026 frontier on local AI agents converges on the same
gap: every agent that "remembers" does so by bolting on a vector
database, a JSON store, or a longer in-context window. None of
these are part of the agent's verified safety surface; none of
them produce a per-event receipt that the gate can audit; none of
them carry an explicit forgetting budget; and all of them
introduce additional dependencies (Pinecone, FAISS, JAX,
LangChain, Mem0, Letta) that re-open the supply-chain attack
surface that v60..v67 spent eight kernels closing.

The cognitive-science literature has named the missing pieces for
decades:

- **Surprise-gated writes.** The hippocampus does not write every
  observation; it writes those that exceed a novelty threshold.
  Bolted-on memories write everything (or nothing, depending on
  the LLM's mood).
- **Activation decay.** Chunks fade with retrieval latency.
  Bolted-on memories never decay; they grow until eviction.
- **Pattern separation + completion.** Hippocampal CA3 does both
  via sparse distributed representations. Vector DBs do dense
  retrieval; pattern *separation* is implicit in embedding
  geometry, *completion* is implicit in nearest-neighbour search.
- **Sleep replay / consolidation.** High-surprise episodes are
  reactivated offline and consolidated into long-term storage.
  Bolted-on memories have no sleep cycle; they are perpetually
  awake.
- **Anti-catastrophic forgetting.** EWC-style ratchets prevent
  new learning from over-writing critical past learning.
  Bolted-on memories have no such ratchet.

We collapse all five into one branchless integer kernel.

## 2. Design

### 2.1 Bipolar HV at D=8192 bits

The episodic store is a ring buffer of `cos_v68_episode_t`,
each containing a single bipolar HV
(`uint64_t w[128]`), a Q0.15 surprise score, a Q0.15
activation, a 32-bit timestamp, and a validity flag.
Pattern separation is the natural sparseness of random bipolar
HVs at this dimension; pattern completion is `__builtin_popcountll`
Hamming → Q0.15 similarity.

### 2.2 Surprise gate (Titans 2025)

```c
uint8_t cos_v68_surprise_gate(int16_t pred, int16_t obs,
                              int16_t threshold,
                              int16_t *out_surprise);
```

A single integer compare: `|obs − pred| ≥ threshold`. The
absolute value is computed branchlessly (`(v + m) ^ m`); the
result is clamped to Q0.15. Writes only happen when the gate
returns 1.

### 2.3 ACT-R activation decay

```c
int16_t cos_v68_actr_decay_q15(int16_t init, int16_t decay,
                               uint32_t dt);
```

`A_t = max(0, A_0 − decay·dt)` computed as a single saturating
integer subtract. We deliberately depart from Anderson's
original log-decay because the integer linear approximation is
(a) branchless, (b) constant-time, (c) easy to reason about
under sanitizers, and (d) good enough for a hippocampal ring
buffer of N ≤ 256 (we never observe enough decay history for
the log term to matter).

### 2.4 Content-addressable recall with rehearsal

```c
uint32_t cos_v68_recall(cos_v68_store_t *s, const cos_v68_hv_t *q,
                        cos_v68_recall_t *out, int16_t *fidelity);
```

Linear scan; per-episode Hamming via four
`__builtin_popcountll` calls per HV; branchless top-K bubble
insertion using `sel_i32` swaps. Accessed episodes have their
activation refreshed to `COS_V68_ACT_INIT_Q15` (the rehearsal
effect — recently retrieved memories are easier to retrieve
again).

### 2.5 Hebbian online adapter (TTT)

```c
int16_t cos_v68_hebb_update(cos_v68_adapter_t *a, const int16_t *pre,
                            const int16_t *post, int16_t eta);
```

Q0.15 outer-product update: `W[i,j] += sat((eta · pre[i] · post[j])
>> 30)`. The actual `eta` is clamped to the adapter's rate
ratchet, which **never grows** between sleeps — the
EWC-discipline encoded as a monotonic per-call invariant. The
inner loop is data-independent; saturation does not early-exit.

### 2.6 Sleep replay / consolidation

```c
uint32_t cos_v68_consolidate(cos_v68_store_t *s,
                             cos_v68_adapter_t *adapter,
                             int16_t keep_thresh,
                             cos_v68_hv_t *lt_out);
```

Per-bit sum across qualifying episodes; bits with non-negative
sum are set in the long-term HV. This is a discrete majority
XOR — the bipolar bundle operator extended to many operands.
Sleep also resets the adapter's rate ratchet to `RATE_INIT`
(the only event that does so), modelling the systems-
neuroscience observation that consolidation creates capacity
for fresh learning.

### 2.7 Forgetting controller

```c
uint32_t cos_v68_forget(cos_v68_store_t *s, int16_t keep_thresh,
                        uint32_t forget_budget);
```

Drops episodes whose activation falls below `keep_thresh`,
capped at `forget_budget` per call. The cap is what makes
forgetting safe — a runaway program cannot wipe the store.

### 2.8 MML — Mnemonic Memory Language

10 opcodes, 8-byte instructions:

```
HALT  SENSE  SURPRISE  STORE  RECALL  HEBB
CONSOLIDATE  FORGET  CMPGE  GATE
```

Each opcode has a fixed integer cost. `GATE` writes
`v68_ok = 1` iff:

- `cost ≤ cost_budget`
- `reg_q15[a] ≥ imm`
- `forget_count ≤ forget_budget`
- `abstained == 0`

A single bytecode program can therefore guarantee
*simultaneously* that recall ≥ threshold and forgetting ≤
budget, with one branchless AND.

## 3. Composition

`cos_v68_compose_decision(v60..v68)` is a 9-way branchless AND.
Each lane is evaluated; the verdict is the product. v68 is one
factor — recall fidelity ≥ threshold AND forget budget honoured
AND learning-rate ratchet stable. If v68 is `0`, the verdict is
`0` regardless of the other lanes.

## 4. Tests

2 669 deterministic assertions, including:

- Full 512-row truth table for the 9-bit composed decision
- 1 024 random-input cross-checks for compose vs brute-force AND
- HV Hamming + similarity + bind involution + permutation
  popcount invariants
- Surprise gate threshold + monotonicity
- ACT-R decay linearity, saturation, no-op
- Episodic store: write/decay/recall round-trip, eviction by
  lowest activation, surprise-gated no-write
- Recall under noise: identical episode under 32 random
  bit-flips is still top-1 with fidelity > 30 000 / 32 767
- Hebbian: eta clamping, saturation, ratchet floor
- Sleep consolidation: high-activation bundle is close to base,
  far-from-base candidate is excluded, ratchet is reset
- Forgetting: budget cap enforced, full drain works
- Storm test: 1 000 random writes never exceed capacity
- MML: 7-instruction program reaches GATE with `v68_ok = 1`;
  tight-budget program abstains; below-threshold program
  produces `v68_ok = 0` even with budget

All pass under ASAN and UBSAN.

## 5. Benchmarks

Apple M-series performance core, `clang -O2`:

| Operation | Throughput |
|---|---|
| HV Hamming, D=8 192 | 38.62 M cmps/s |
| Recall, N=256, D=8 192 | 110.54 k q/s |
| Hebbian update, 16×16 | 24.03 M upd/s |
| Sleep consolidation, N=256 | 3.77 k cons/s |
| MML 5-instruction program | 38.39 M progs/s (~192 M ops/s) |

## 6. Limitations

- Recall is a linear scan; not appropriate for store sizes ≫ 1k
  (use an external vector DB if you need that scale, but you
  lose the integer-only + receipts properties).
- The Hebbian adapter is a small Q0.15 matrix; not a transformer
  parameter update. Larger adapters are out of scope.
- ACT-R linear decay is an approximation to the original log
  decay; under decade-long retention horizons the approximation
  diverges. We do not target those horizons.
- Sleep consolidation is currently called explicitly by the
  agent; an automatic scheduler would be a v69 follow-up.

## 7. Related work

- **Titans** (Google Research, 2025; arXiv:2501.00663) — neural
  memory module with surprise-gated test-time writes. v68 ports
  the surprise gate into a single branchless integer compare.
- **TTT** (Sun *et al.*, arXiv:2407.04620) — test-time training.
  v68 ports the inner update loop into a saturating Q0.15
  outer-product.
- **EWC** (Kirkpatrick *et al.*, 2017) and **ASAL** (DeepMind,
  2025) — anti-catastrophic-forgetting via Fisher-information
  weighted regularisation. v68 ports the *invariant* (learning
  rate cannot exceed past learning's stability budget) without
  the parameter-server machinery.
- **ACT-R / Soar / LIDA** — declarative chunks with activation
  decay and rehearsal. v68 ports the activation algebra into
  Q0.15 integers.
- **Diekelmann & Born (Nat Rev Neurosci 2010)** + 2024 systems
  extensions — sleep consolidation strengthens high-surprise
  traces. v68 ports this into majority-XOR bundle of
  high-activation episodes.
- **MemGPT / Letta** (2024-2026) — paged memory for LLMs. v68
  is not paged memory; it is *episodic* memory with a sleep
  cycle and an EWC ratchet.

**1 = 1.**

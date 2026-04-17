# v68 σ-Mnemos — Positioning

How σ-Mnemos differs from the 2026 continual-learning + episodic-
memory + agent-memory frontier.

| | Titans (Google 2025) | LangMem / Mem0 (2026) | Letta / MemGPT 2026 | Pinecone / Weaviate | Voyager (NVIDIA) | **σ-Mnemos (v68)** |
|---|---|---|---|---|---|---|
| Substrate | Transformer + neural memory module | LLM + JSON store | LLM + chunked context manager | Vector DB | LLM + skill library | **C, integer-only, libc-only** |
| Surprise-gated writes | yes (paper) | no | no | no | implicit | **yes — single branchless compare** |
| Test-time training | weights | retrieval prompt | summary prompt | retrieval prompt | code-skill addition | **Hebbian Q0.15 outer-product** |
| Anti-forgetting | replay | none | summarisation | none | skill-library growth | **EWC-style rate ratchet (never grows)** |
| Sleep / consolidation | implicit (parameter update) | none | none | none | none | **explicit majority-XOR bundle** |
| Forgetting budget | none | none | none | none | none | **branchless cap, MML-enforced** |
| Decision lane integer-only | no | no | no | no | no | **yes — Q0.15 everywhere** |
| Side-channel-free recall | no | no | no | no | no | **popcount-Hamming, constant in data** |
| Receipts per memory event | none | log line | log line | none | none | **MML cost + GATE per program** |
| Composes with security stack | bolted | bolted | bolted | external | bolted | **9-bit branchless AND with v60..v67** |
| Dependencies | Python + JAX | Python + LLM | Python + LLM | external service | Python + LLM | **libc only** |
| Dependency-free? | no | no | no | no | no | **yes** |
| LOC for the kernel | 1 000s + JAX | 10 000s | 10 000s | external | 1 000s + LLM | **~700 in one file** |

## What's unique

1. **Single C file, libc-only.** No JAX, no FAISS, no Pinecone, no
   LangChain. The whole continual-learning + episodic-memory plane
   fits in `mnemos.c` and `mnemos.h`. Nothing is imported.

2. **Integer-only on every decision surface.** Q0.15 throughout —
   surprise gate, ACT-R decay, recall similarity, Hebbian update,
   consolidation threshold, forgetting threshold, MML registers,
   GATE compare. No FP, anywhere, on any path that can affect
   `v68_ok`.

3. **Branchless on the data.** Top-K recall is a branchless
   bubble-pass with `sel_i32` swaps. Surprise gate is one integer
   compare. Decay is `max(0, A − decay·dt)` computed without
   `if`. Hebbian update is independent of weight magnitude (no
   early-exit when a cell saturates).

4. **Receipts per memory event.** Every MML program tracks
   per-instruction memory-unit cost. The GATE opcode writes
   `v68_ok = 1` iff `cost ≤ budget AND recall ≥ threshold AND
   forget_count ≤ forget_budget AND NOT abstained`. **No
   continual-learning step crosses to the agent without a budget
   receipt and a recall receipt and a forget-budget receipt.**

5. **Anti-catastrophic-forgetting by construction.** The Hebbian
   learning-rate ratchet (`cos_v68_adapter_ratchet`) **never
   grows** between sleeps. The kernel cannot adapt past
   saturation, and the rate floor is hard-coded at
   `COS_V68_RATE_FLOOR_Q15 ≈ 0.0156`. Sleep consolidation is the
   only event that resets the ratchet.

6. **Forgetting is opt-in and budget-capped.** No call to
   `cos_v68_forget` can drop more than `forget_budget` episodes,
   regardless of how many qualify. A runaway program cannot wipe
   memory.

7. **Composes with the security stack.** v68 is one factor in a
   9-way branchless AND alongside v60..v67. There is no separate
   "memory layer" with its own opaque trust assumptions — the
   memory plane is part of the same composed decision as the
   sandbox, the lattice, the AEAD envelope, and the deliberation
   beam.

8. **Hyperdimensional space at silicon speed.** Episodic store is
   bipolar HVs at D=8192 bits, recalled by
   `__builtin_popcountll` Hamming. ~38 M comparisons per second
   on a single Apple M-series performance core, no GPU, no
   accelerator.

## What it deliberately is not

- Not a vector DB. There is no IVF, HNSW, scalar quantisation, or
  index rebuild. Recall is a linear scan over a bounded ring
  buffer. The expected use case is hippocampus-sized (≤ 256
  episodes per session, consolidated to a single long-term HV
  during sleep) — not corpus-sized.
- Not a parameter-update mechanism for a transformer. The
  Hebbian adapter is a small `R × C` Q0.15 matrix that the
  surrounding agent reads as bias. Bigger adapters are out of
  scope (use v66 σ-Silicon for matrix workloads).
- Not a skill library. v64 σ-Intellect already ships
  Voyager-style skill curation; v68 is the *memory of skill
  use*, not the catalogue.

**1 = 1.**

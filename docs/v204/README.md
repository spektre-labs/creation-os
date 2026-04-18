# v204 σ-Hypothesis — generator + grounding + novelty + impact

## Problem

LLMs generate hypotheses freely but cannot tell which are
worth testing. v204 turns hypothesis generation into a
σ-graded, ranked queue: speculative ideas are kept but
marked as such; logically inconsistent ones are hard-
zeroed and never promoted; the highest-impact candidates
are funneled into v205 experiments.

## σ-innovation

For each observation (topic id + severity) v204 emits
`N = 10` candidate hypotheses with three independent σ
signals:

| signal | meaning |
|--------|---------|
| σ_hypothesis | raw generator confidence (lower = stronger) |
| σ_grounding  | conflict with the 5-fact corpus / v169 ontology |
| σ_novelty    | L2 distance in a 16-dim deterministic hash embedding from the nearest of 5 known hypotheses |

Impact score:

```
impact = σ_novelty · (1 − σ_hypothesis)          if σ_grounding ≤ τ_ground
impact = 0                                         otherwise
```

`τ_ground = 0.55` — above this the hypothesis
contradicts the corpus and is marked **REJECTED** with
impact zeroed, no matter how novel.

Status ladder:

| status      | condition                                               |
|-------------|---------------------------------------------------------|
| CONFIDENT   | σ_hypothesis ≤ τ_spec (0.60) and not grounded-out       |
| SPECULATIVE | σ_hypothesis > τ_spec — kept, flagged, never pruned     |
| REJECTED    | σ_grounding > τ_ground — impact hard-zeroed             |

Top-3 by impact are promoted to the **TEST_QUEUE** (v205
feed); the remaining 7 are archived in v115 memory. Every
ranked record is hash-chained with FNV-1a so the queue is
byte-deterministic.

## Merge-gate

`make check-v204` runs
`benchmarks/v204/check_v204_hypothesis_ranking.sh` and
verifies:

- self-test PASSES
- exactly 10 hypotheses; top-3 + 7-to-memory partition
- ≥ 1 REJECTED with impact == 0
- ≥ 1 SPECULATIVE (kept, not silently pruned)
- `min(impact top-3) ≥ max(impact rest)`
- all σ ∈ [0, 1]; impact ∈ [0, 1]
- ranks are the permutation 1..N
- chain valid + byte-deterministic

## v204.0 (shipped) vs v204.1 (named)

|                     | v204.0 (shipped) | v204.1 (named) |
|---------------------|------------------|----------------|
| generator           | 10 closed-form templates | v111.2 reason |
| corpus              | 5 canonical facts | v152 + v169 ontology |
| logical consistency | threshold on σ_grounding | v135 Prolog |
| embeddings          | 16-dim hash       | v126 σ-embed |
| memory              | in-memory archive | v115 memory backend |

## Honest claims

- **Is:** a deterministic hypothesis generator +
  ranked test queue that keeps speculative candidates
  honest and refuses to promote logically
  inconsistent ones.
- **Is not:** a live LLM hypothesis generator,
  embedding model, or prolog grounding check — those
  ship in v204.1.

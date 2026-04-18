# v126 σ-Embed — σ-aware embedding for the v115 memory store

> v115 memory already ranks memories with
> `cosine(q,m) / (1 + λ · σ_product(m))` — v126 makes σ part of the
> cosine itself.

## Problem

`v115.0` uses a 384-dim hash-shingled embedding as a MiniLM stand-in.
It works as a shape-correct placeholder, but it collapses memories
with identical content and different σ_profiles into the same
neighborhood: two remembered answers to the same question are one
vector to the retriever, even if one was written while the model was
99 % confident and the other while it was wobbling.

Generic off-the-shelf embedders (all-MiniLM-L6-v2, E5, BGE) have
the same blindness.  They don't know about σ because nothing in the
typical RAG stack carries σ end-to-end.  Creation OS does.

## σ-innovation: hybrid 2568-dimensional embedding

```
v126_embed(x) = [  hidden_2560(x)  |  σ_weight · σ_8(x)  ] ∈ ℝ^2568
```

- **`hidden_2560(x)`** — BitNet layer-15 hidden state.  The
  generator *is* the retriever, no separate embedding network.
  v126.0 ships a deterministic hash-shingle projector as a stand-in
  so the merge-gate is weights-free; v126.1 swaps in the actual
  BitNet extractor behind the same API.
- **`σ_weight · σ_8(x)`** — v101's 8-channel σ_profile scaled by
  `sigma_weight` (default 0.50) and concatenated.  Because cosine
  is over the *whole* 2568-d vector, two memories with identical
  content but divergent σ are no longer at cosine 1.

Ranking still uses v115's weighting:

```
score(q, m) = cosine_2568(q, m) / (1 + λ · σ_product(m))
```

so "uncertain memories are down-weighted, not silently mixed into
context" remains true — now with σ also *inside* the dot product.

## Contracted properties (measured by `make check-v126`)

| property | expectation |
|---|---|
| embedding dimension | exactly `2568 = 2560 + 8` |
| identical content + identical σ | cosine = 1.0 |
| identical content + divergent σ | `0.80 < cosine < 1.0` |
| unrelated content + same σ | `cosine < 0.40` (content dominates) |
| rank with 3 memories | same-topic low-σ > same-topic high-σ > off-topic |
| σ_product of a profile | geometric mean of the 8 channels |
| hidden block L2-norm | = 1.0 before σ concatenation |

Example run from the merge-gate:

```
ranking = [
  {index:0, score:0.878, σ_product:0.10},   ← in-domain low-σ
  {index:1, score:0.481, σ_product:0.85},   ← in-domain high-σ (down-weighted)
  {index:2, score:0.026, σ_product:0.05},   ← off-topic low-σ
]
```

Uncertainty of the memory directly enters the ranking.

## What ships in v126.0

- `cos_v126_hidden_from_text` — deterministic hash-shingle
  projector (FNV-1a + SplitMix64, 4-slot shingle, L2-normalized).
- `cos_v126_sigma_fill` — builds a σ_profile from 8 channels and
  caches the geometric-mean σ_product.
- `cos_v126_embed` — composes the 2568-d vector.
- `cos_v126_cosine` / `cos_v126_score` / `cos_v126_rank_topk`.
- CLI: `--self-test | --cosine | --rank-demo`.
- Merge-gate smoke: dimension, identical/divergent/off-topic
  cosine ranges, and top-k rank pinning.

## What lands in v126.1

- Real BitNet layer-15 extraction through the v101 bridge,
  returning the natural 2560-d hidden state.  The header API does
  not change, so v115 memory, v117 long-context, and v116 MCP all
  pick up the upgrade by recompiling.
- Benchmark against MiniLM-L6-v2 on a v115 memory recall suite,
  archived in `benchmarks/v126/results/`.

## Source

- Header    : [`src/v126/embed.h`](../../src/v126/embed.h)
- Impl      : [`src/v126/embed.c`](../../src/v126/embed.c)
- CLI       : [`src/v126/main.c`](../../src/v126/main.c)
- Smoke     : [`benchmarks/v126/check_v126_embed_smoke.sh`](../../benchmarks/v126/check_v126_embed_smoke.sh)

## Self-test

```
make check-v126
```

## Claim discipline

v126 is a **Lab demo (C)**: the embedding *composition, cosine, and
rank contract* are measured today with a hash-shingle projector.
The claim that it is "better than MiniLM" is not made anywhere in
this tree until v126.1 produces a measured recall comparison.
What we do claim today: **σ contributes to the cosine**, which
nobody else's RAG stack does — and the self-test proves it.

## σ-stack layer

**Retrieval / Memory** — sits under v115 (episodic memory) and
upstream of v117 (long-context paging) through v115's existing
`embed_fn` indirection.  A v115 rebuild against the v126 header
makes the whole stack σ-aware without schema churn.

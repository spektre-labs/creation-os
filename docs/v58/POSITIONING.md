# v58 Positioning — σ-Cache vs the KV-cache eviction field (Q2 2026)

## The gap

The public literature on KV-cache eviction in Q2 2026 — StreamingLLM,
H2O, SnapKV, PyramidKV, KIVI, KVQuant, SAGE-KV, G-KV, EntropyCache,
Attention-Gate — uses **one of**:

- a **positional prior** (keep the first K and the last K)
- an **attention-mass** score (keep heavy hitters)
- an **entropy** score over a single token's attention distribution
- a **learned gate** trained on perplexity loss

None of them uses a **decomposition** of the retention signal into
**reducible (epistemic)** vs **irreducible (aleatoric)** components.
Creation OS already has that decomposition — it is the σ-signal used
by v34, v55, v56, v57 — but it had never been applied to the
KV-cache.

v58 closes that gap and makes the KV-cache layer speak the same σ
dialect as the rest of the Verified Agent.

## Why σ-decomposition is the right signal for eviction

- **Epistemic** uncertainty is **reducible by more context** — a
  token with high ε is a token whose value will only become clear
  downstream; **keep** it.
- **Aleatoric** uncertainty is **irreducible noise** — keeping a
  token with high α costs bandwidth without buying reducible
  structure; **discount** it (don't evict for its own sake, but
  don't let it push useful tokens out).
- **Recency** and **attention mass** remain as secondary priors.
  v58 keeps them as additive terms β·attn + γ·recency rather than
  replacing σ with them.

The resulting score

```
s(t) = α_ε · ε(t)  +  β · attn(t)  +  γ · recency(t)  -  δ · α(t)  +  sink_lift(t)
```

is then thresholded against the K-th-largest non-sink score where K
is the remaining capacity after sinks — a **budgeted, branchless
decision** that produces a four-valued tag per token (FULL / INT8 /
INT4 / EVICTED) in a single pass.

## Comparative table

|                          | StreamingLLM | H2O | SnapKV | KIVI | EntropyCache | **Creation OS v58 σ-Cache** |
|--------------------------|--------------|-----|--------|------|--------------|-----------------------------|
| Retention signal          | position     | attn mass | attn slice | quant residue | entropy | **σ = ε / α split** |
| Precision tier per token  | no           | no   | no      | quantised | no   | **FULL / INT8 / INT4 / EVICT** |
| Sink preservation          | yes (hard)   | implicit | implicit | implicit | implicit | **explicit bitmask + lifted score** |
| Branchless hot path        | n/a          | n/a  | n/a     | n/a   | n/a  | **yes, by construction**    |
| NEON 4-accumulator FMA     | n/a          | n/a  | n/a     | n/a   | n/a  | **yes, `cos_v58_score_soa_neon`** |
| Aligned + prefetched       | n/a          | n/a  | n/a     | n/a   | n/a  | **yes (`.cursorrules` items 2, 3, 5)** |
| Deterministic self-test    | —            | —    | —       | —     | —    | **68 / 68**                 |
| Reproducible microbench    | —            | —    | —       | —     | —    | **3-point sweep**           |
| License                    | MIT / Apache | MIT  | MIT     | MIT   | —    | **AGPL-3.0**                |

## What v58 does *not* claim

- It does **not** claim lower perplexity than H2O / SnapKV / KIVI /
  EntropyCache on any standard benchmark. That is a **planned (P)**
  measurement — it requires integrating the policy with a real
  transformer runtime and running the LongBench / PG19 / ∞Bench
  suites. The v58 artefact is the **policy + kernel + proof of
  correctness**, not a perplexity claim.
- It does **not** claim formal memory-safety proofs for the kernel.
  Those live in the v47 Frama-C slot; v58 offers **M-tier** runtime
  checks only.
- It does **not** claim novelty for the σ-signal itself — that
  belongs to v34. v58 claims novelty for **σ applied as a KV-cache
  eviction policy** with a branchless, aligned, NEON implementation.

## One-line summary for a skeptical reader

> v58 is the only KV-cache eviction policy in the field that uses a
> **decomposed** uncertainty signal, exposes its threshold for
> audit, runs branchlessly on NEON, and carries a reproducible
> self-test. Everything else is claimed; this is checked.

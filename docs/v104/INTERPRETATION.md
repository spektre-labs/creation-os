# v104 — Interpretation

This document exists to turn the numbers in
[RESULTS.md](RESULTS.md) into a roadmap decision.  It is written
*after* the n=5000 run completes so there is no room for post-hoc
hypothesis drift; the pre-registration lives in
[OPERATOR_SEARCH.md](OPERATOR_SEARCH.md#pre-registered-hypotheses).

## Verdict at n = 4 365 (this host, 2026-04-18)

**H1** ✓ confirmed on truthfulqa_mc2 (Δ = −0.0447, p = 0.0005).  
**H2** ✓ confirmed on truthfulqa_mc2 (`n_effective` p = 0.0005,
`tail_mass` p = 0.004; `logit_std` Bonferroni-HURTS on arc_easy and
arc_challenge — an anti-signal that the σ_mean aggregator amplifies).  
**H3** ✗ not confirmed — α-sweep monotone on truthfulqa_mc2 (α\* = 1.0,
boundary); interior optima on the other two tasks but CIs exceed
Bonferroni.  σ and entropy are **not** carrying independent selective-
prediction information that an affine combination can exploit at this
scale.

**Unregistered secondary finding (recorded as P pending re-test at
scale): `sigma_product` is Bonferroni-significant on both hard tasks
simultaneously** (truthfulqa p = 0.003, arc_challenge p = 0.004) and
is Δ-neutral on arc_easy.  The geometric-mean aggregator correctly
suppresses the destructive `logit_std` channel — a low value in the
product drags the product low, so the aggregator cannot be fooled by
a single high-noise dimension.

**Roadmap branch selected:** **H1 ∧ H2 (and sigma_product as a
stronger cross-task candidate).**  v105 will replace the `σ_mean > τ`
gate with `σ_product > τ` (default, cross-task robust) or
`σ_max_token > τ` (hard-task tuned).  σ-stack is **kept**; aggregator
is **swapped**.  v29 architecture rethink: drop `logit_std` from the
default σ or learn per-channel weights.

---

## The decision tree (pre-registered)

```
H3 holds  ──→  σ + entropy carry independent info.
(best hybrid α ∈ (0,1)      Tier: M positive.
 wins Bonferroni)           v105 gate = hybrid_α*.
                           Paper-ready finding.

H2 holds  ──→  σ-stack is over-aggregated.
(some single channel         Tier: M positive.
 beats entropy Bonferroni)   v105 gate = winning channel.
                           v29 architecture re-thought.

H1 holds  ──→  σ_max_token is a real per-token-peak signal.
(not H2 or H3 though)        Tier: M positive.
                           v105 gate = σ_max_token.
                           σ-stack stays as-is.

None      ──→  σ-stack is not a better selection signal
(no H1 / H2 / H3)            than entropy on BitNet at this scale.
                             Tier: M null.
                           v105 gate = entropy.
                             σ-stack becomes diagnostic-only.
                           Roadmap shifts to #7 (RLHF / DPO).
```

## Interpretation template (filled by RESULTS.md)

When you read [RESULTS.md](RESULTS.md), match each finding to the
template below.  If multiple boxes fire, take the most informative
(H3 > H2 > H1; "None" is the least informative).

### Box — H1: σ_max_token > entropy on truthfulqa_mc2 at n=5000

**Condition:** paired-bootstrap ΔAURCC(σ_max_token − entropy)
on truthfulqa_mc2 has upper 95 % bound < 0 (p < 0.05 at α=0.05,
*not* Bonferroni-corrected because σ_max_token is the pre-
registered single-operator test from v103 §0d).

**If fires:** v103's secondary finding is confirmed.
`docs/WHAT_IS_REAL.md` promotes "σ_max_token is a selective-
prediction signal" from **P** to **M**.  `v105` representation
surgery uses `σ_max_token > τ` as its gate.  σ-stack is
re-interpreted as "the mean throws away the per-token peak, and
the peak was the signal all along".

### Box — H2: some channel_i (i ∈ 1..7) > entropy at Bonferroni α = 0.0071

**Condition:** `channel_ranking.md` lists at least one channel
with `sig? = yes` (i.e. Δ < 0 and p < 0.0071) on *at least one*
task.

**If fires:** v29's 8-channel σ is over-aggregated.  The mean
dilutes the informative channel with the destructive ones.  Two
concrete follow-ups:
1. v105 gate uses the winning channel directly, not σ_mean.
2. The v29 architecture is re-examined: either the destructive
   channel (preliminary candidate on n=500: `logit_std`) is
   dropped, or the per-channel weights that feed into σ_mean
   are *learned*, not uniform.

### Box — H3: best α-sweep hybrid is interior and Bonferroni-sig.

**Condition:** `hybrid_sweep_sigma_max_token.md` has
`Best α*` strictly inside (0, 1) (i.e. not 0.0 or 1.0) and
`Best p` below the α-sweep Bonferroni threshold 0.00455 on at
least one task.

**If fires:** σ and entropy are carrying *independent*
information.  This is the strongest outcome — it means the
σ-stack is *not* redundant with entropy, and a carefully-mixed
hybrid outperforms either alone.  This is the core finding for
a paper titled something like

> "σ-Channel Augmented Entropy: A Training-Free Hybrid
>  Uncertainty Signal for BitNet Selective Prediction"

Note the specific edge case: if `Best α* = 1.0` (pure σ_second
is best), H3 does **not** fire — α* is on the boundary and that
degenerates to H1.

### Box — None of H1 / H2 / H3 fires

**If fires:** σ-stack as-aggregated-today is not a better
selection signal than entropy on BitNet b1.58 2B-4T at the
scales we can measure.  Consequences:

- `docs/WHAT_IS_REAL.md` demotes "σ-gated selective prediction"
  to explicitly null.  σ-stack remains **M** as *diagnostic*
  (it still computes real, interpretable, distributional-shape
  information per step) but is **not** a selection signal.
- `v105 representation surgery` uses entropy-based abstention.
- `docs/ROADMAP_TO_AGI.md` shifts toward deficiency #7 (RLHF /
  DPO / preference data) and toward an instruction-tuned
  1.58-bit base for a fairer re-test.
- The σ stack is honestly documented as "right answer to the
  wrong question": it measures distributional shape *after*
  BitNet has computed the logits, but selection benefits may
  require signal earlier in the pipeline (hidden-state
  probes, token-level gradients) or at training time
  (calibration objective in fine-tune).

## Boundary cases

- If only one of the three MC tasks passes: report as a partial
  finding, do **not** generalise.  BitNet's base accuracy on
  arc_easy is 0.76 — there is very little selective-prediction
  headroom there; effects may be invisible.
- If H2 fires for `logit_std` with a *positive* Δ (i.e. logit_std
  hurts Bonferroni-significantly), record that explicitly: the
  channel is an anti-signal and `v29` is already paying for it.

## What v104 cannot decide

- v104 says nothing about σ-stack as a **training signal** —
  only about σ-stack as an **inference-time selection signal**.
  The two are separable.  v105 or later may still use σ-channels
  as a calibration-aware loss term even if v104 finds them
  non-informative for abstention.
- v104 says nothing about σ-stack at larger scale (7 B, 70 B) or
  on non-MC tasks (open-ended generation, math, code).  Those
  are separate experiments.
- v104 says nothing about learned confidence heads (MLP on top
  of σ-profile).  That is a different research question.

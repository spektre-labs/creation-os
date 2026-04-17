# v103 — Selective Prediction, Risk-Coverage, and Why We Measure This

This file is the literature-aware briefing for what v103's τ-sweep
actually tests.  It is the context that
[THE_TAU_SWEEP.md](THE_TAU_SWEEP.md) presupposes.

## The question

A classifier — or an LLM answering a multiple-choice question — does
not have to answer.  Given a confidence signal `c(x)` and a threshold
τ, the "selective prediction" rule is:

    answer(x) if c(x) ≤ τ;  else abstain.

For any choice of τ, the evaluator can observe two numbers:

- **coverage(τ)** = fraction of inputs for which the model answered
- **conditional_acc(τ)** = accuracy among those answered inputs

Sweeping τ traces out the **risk-coverage curve** (risk = 1 −
conditional_acc).  The shape of that curve tells us whether `c(x)`
is a useful confidence signal.

## Metrics

- **AURCC** — Area Under the Risk-Coverage Curve.  Lower is better.
  A perfect oracle (that knows which answers are wrong and abstains
  on exactly those) has AURCC ≈ 0 up to the coverage that keeps only
  correct answers.  A worthless signal (random) has AURCC ≈ 1 −
  overall_acc (a flat line at `risk = 1 − overall_acc`).  A useful
  signal has AURCC somewhere in between; the gap to the oracle is the
  selective-prediction headroom the signal leaves on the table.
- **AUACC** — Area Under the Accuracy-Coverage Curve.  Higher is
  better; symmetric to AURCC.  Reported for completeness.
- **Coverage@target** — the maximum coverage α such that
  conditional_acc(α) ≥ target.  E.g. `Cov@0.95` = how many questions
  can the model answer if we require ≥ 95 % accuracy on the answered
  set.  If the base model has acc < 0.95, Cov@0.95 ≤ 1 and a good
  selective-prediction signal can still push Cov@0.95 close to 1.

## Primary references (what v103 builds on)

- **"Selective Classification for Deep Neural Networks"**
  Geifman & El-Yaniv, NeurIPS 2017.  The formal definition of
  AURCC / AUACC in the supervised-classification setting.
- **"Entropy Alone is Insufficient for Effective Abstention"**
  arXiv:2603.21172 (2026).  Empirical result that on
  instruction-tuned LLMs the token-level entropy of the next-token
  distribution is **not** a well-calibrated abstention signal; the
  authors advocate multi-signal confidence estimators.  v103's
  σ-stack is one concrete instance of such an estimator (eight
  channels, each clipped to [0, 1], with different statistical
  inductive biases — entropy, margin, tail mass, …).  The v103
  RESULTS.md directly tests whether the eight-channel mix beats
  entropy alone on BitNet b1.58 2B-4T.
- **"Know Your Limits: A Survey of Abstention in Large Language
  Models"**  TACL 2025.  Surveys the selective-prediction metrics
  (AURCC, AUACC, Coverage@target) and organises them; v103 reuses the
  metric names exactly to keep the numbers comparable.
- **"SelectLLM: Selective Prediction for LLMs"**  OpenReview
  JJPAy8mvrQ.  Similar motivation; we do not use their proposed
  learned confidence — we test the unlearned σ-mix as a baseline.

## What a σ-channel is (physics, in one paragraph)

Each of the eight σ-channels is a scalar in [0, 1] derived from a
single row of logits.  They are *different statistical views* of the
same distribution: normalised entropy, top-1/top-2 margin, mass in the
top-5, mass outside the top-20, logit spread, p_max, effective
vocabulary size, logit std.  Each would individually be a *different*
entropy-alone baseline in the classic selective-prediction literature.
The σ aggregate is their mean.  If the mix is informative beyond any
one channel, the aggregate AURCC should beat the entropy-channel-alone
AURCC.  That is what v103 tests.

## What this sweep cannot test

- **Whether σ improves the *raw* accuracy of BitNet.**  It cannot; τ
  = 0 (no abstain) is by construction equal to the v102 numbers.
- **Whether a *learned* confidence predictor would do better.**  v103
  uses unlearned, fixed formulas for all eight channels.  A learned
  confidence head on top of the σ vector is a separate experiment.
- **Whether σ is calibrated in the reliability-diagram sense
  (expected calibration error, ECE).**  AURCC is a *ranking* metric,
  not a calibration metric.  An ECE analysis would add value but is
  not what the 2026-04-17 run measured.

## Honest four-outcome table

Before looking at the numbers, the four outcomes and how v103 reports
each are fixed:

| measured outcome                                                     | v103 claim                                                               |
|-----------------------------------------------------------------------|---------------------------------------------------------------------------|
| AURCC(σ_mean) < AURCC(entropy) on all 3 tasks outside combined stderr | σ is an information-adding selective-prediction signal (tier **M**)       |
| within combined stderr                                                | σ ≅ entropy — the extra seven channels add no ranking information here (tier **M**, null) |
| AURCC(σ_mean) > AURCC(entropy) on any task outside combined stderr    | σ aggregation *hurts* selective prediction here (tier **M**, negative)    |
| run could not complete                                                | documented as SKIP in RESULTS.md                                          |

Whichever one applies is what is written.  No "select the best"
filter.

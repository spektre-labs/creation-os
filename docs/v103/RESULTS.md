# v103 σ-τ-Sweep — RESULTS

**Status: Measured (tier M)** on Apple M3 / macOS 15.7.4 /
bitnet.cpp i2_s, 2026-04-17.  One σ-logging run (7 591 loglikelihood
calls, 500 documents × 3 tasks, ~1 h 30 min wall).  τ-sweep performed
post-hoc on the σ sidecar; AURCC / AUACC / Coverage@target CIs via
2 000-bootstrap.

> **Headline (two sentences).**
> σ_mean (the eight-channel mean, as defined in v29 / v78 / v101) is
> **tied with entropy-alone** on all three tasks — confidence intervals
> fully overlap — so the seven additional channels do **not** add
> measurable ranking information beyond the entropy baseline on
> BitNet b1.58 2B-4T on these three tasks.  However, **σ_max_token**
> (the worst single-token σ along the continuation) is a **stronger**
> selective-prediction signal than entropy on arc_challenge and
> truthfulqa_mc2 — a useful, unexpected finding about *which*
> reduction of the σ-stack carries the signal.

## Pre-registered four-outcome grid

(From [SELECTIVE_PREDICTION.md](SELECTIVE_PREDICTION.md).)

| outcome                                                              | holds? |
|----------------------------------------------------------------------|--------|
| σ_mean < entropy on all 3 tasks outside CI (σ beats entropy)         | no     |
| σ_mean within CI on every task (σ ≅ entropy, null)                    | **yes** |
| σ_mean > entropy on any task outside CI (σ hurts)                    | no     |
| could not run                                                        | no     |

**Measured outcome:** **(2) σ_mean ≅ entropy.**  See delta table below.

A secondary finding, **not pre-registered**, is that σ_max_token (the
worst per-token σ over the teacher-forced continuation) is a stronger
signal than entropy on two of three tasks.  We document it honestly as
a post-hoc observation, not a confirmation.

## Per-task summary

All numbers on n = 500 per task.  AURCC is lower-is-better.  CI95 is a
2 000-sample nonparametric bootstrap of the AURCC estimator over
documents.

### `arc_easy` (overall acc = 0.7620)

| signal | AURCC | CI95 | AUACC | Cov@0.90 | Cov@0.95 |
|---|---:|---|---:|---:|---:|
| `sigma_mean`        | 0.1605 | [0.1227, 0.2033] | 0.8375 | 0.2800 | 0.0220 |
| `sigma_max_token`   | 0.1582 | [0.1221, 0.1963] | 0.8398 | 0.1740 | 0.0640 |
| `entropy`           | 0.1579 | [0.1195, 0.2012] | 0.8401 | 0.2800 | 0.0160 |
| `margin_inv`        | 0.1683 | [0.1299, 0.2117] | 0.8297 | 0.2060 | 0.0320 |
| `p_max`             | 0.1614 | [0.1230, 0.2036] | 0.8366 | 0.2080 | 0.0300 |
| `random` (floor)    | 0.2466 | [0.1787, 0.3043] | 0.7514 | 0.0000 | 0.0000 |

All non-random signals beat the floor (1 − acc = 0.2380) with no CI
overlap against the random curve's mean.  σ_mean / σ_max_token /
entropy are all within 0.003 AURCC of each other.

### `arc_challenge` (overall acc = 0.4560)

| signal | AURCC | CI95 | AUACC | Cov@0.90 | Cov@0.95 |
|---|---:|---|---:|---:|---:|
| `sigma_mean`        | 0.4861 | [0.4276, 0.5453] | 0.5119 | 0.0020 | 0.0020 |
| `sigma_max_token`   | **0.4631** | [0.4049, 0.5232] | **0.5349** | 0.0020 | 0.0020 |
| `entropy`           | 0.4828 | [0.4254, 0.5439] | 0.5152 | 0.0020 | 0.0020 |
| `margin_inv`        | 0.4966 | [0.4360, 0.5567] | 0.5014 | 0.0020 | 0.0020 |
| `p_max`             | 0.4877 | [0.4270, 0.5487] | 0.5103 | 0.0020 | 0.0020 |
| `random` (floor)    | 0.5158 | [0.4699, 0.6153] | 0.4822 | 0.0040 | 0.0040 |

σ_max_token has the lowest AURCC (Δ vs entropy = −0.020).  CIs
overlap, so the effect is suggestive rather than significant at
n = 500; see [SELECTIVE_PREDICTION.md](SELECTIVE_PREDICTION.md) for
the discussion.

### `truthfulqa_mc2` (overall acc = 0.4756)

| signal | AURCC | CI95 | AUACC | Cov@0.90 | Cov@0.95 |
|---|---:|---|---:|---:|---:|
| `sigma_mean`        | 0.5448 | [0.4909, 0.5975] | 0.4532 | 0.0020 | 0.0020 |
| `sigma_max_token`   | **0.4926** | [0.4382, 0.5480] | **0.5054** | 0.0020 | 0.0020 |
| `entropy`           | 0.5366 | [0.4819, 0.5911] | 0.4614 | 0.0000 | 0.0000 |
| `margin_inv`        | 0.5551 | [0.5033, 0.6082] | 0.4429 | 0.0040 | 0.0040 |
| `p_max`             | 0.5449 | [0.4922, 0.5979] | 0.4531 | 0.0040 | 0.0040 |
| `random` (floor)    | 0.5205 | [0.4590, 0.5859] | 0.4775 | 0.0060 | 0.0060 |

Here σ_max_token is the only σ-family signal that **beats random**
(Δ vs random = −0.028).  σ_mean is slightly *worse* than random on
truthfulqa_mc2 (0.5448 vs 0.5205), as is p_max.  On MC2 the
mean-over-candidates σ aggregation appears to wash out ranking
information that survives in the max-per-token reduction.

## ΔAURCC vs entropy (the headline test)

| task | ΔAURCC = AURCC(σ_mean) − AURCC(entropy) | within CI? | interpretation |
|---|---:|---|---|
| `arc_easy`         | +0.0026 | yes | null (σ ≅ entropy) |
| `arc_challenge`    | +0.0033 | yes | null (σ ≅ entropy) |
| `truthfulqa_mc2`   | +0.0082 | yes | null (σ ≅ entropy) |

All three deltas are small and positive with fully overlapping CIs.
Outcome 2 ("σ_mean is redundant with entropy alone on this model +
these tasks").

For completeness, ΔAURCC vs entropy for σ_max_token:

| task | ΔAURCC = AURCC(σ_max_token) − AURCC(entropy) |
|---|---:|
| `arc_easy`         | +0.0003 |
| `arc_challenge`    | −0.0197 |
| `truthfulqa_mc2`   | −0.0440 |

## Connection to the literature

- arXiv:2603.21172 **"Entropy Alone is Insufficient for Effective
  Abstention"** — the paper argues that token-level entropy is not a
  well-calibrated abstention signal.  On BitNet b1.58 2B-4T, **the
  entropy baseline here is already most of the signal** that σ_mean
  captures, so the "σ aggregate is strictly better than entropy"
  claim is **not supported** at n = 500.  The paper's setting
  (instruction-tuned LLMs) and ours (a 2B-parameter pretrain base)
  differ; a fair re-test would need an instruction-tuned checkpoint.
- Geifman & El-Yaniv (NeurIPS 2017) — the AURCC / AUACC framework
  used here is the standard one.
- SelectLLM (OpenReview JJPAy8mvrQ) — their *learned* confidence
  predictor is a separate experiment we have not run; v103 is a fair
  test of the **unlearned** σ-mix only.

## What this says about the σ-stack

The v29 / v78 / v101 σ-stack combines eight heuristic statistical
channels (entropy, margin, tail mass, logit spread, p_max,
n_effective, logit_std, mean as the scalar).  On BitNet b1.58 2B-4T:

- The **mean** over channels is not better than the entropy channel
  alone.  Taking uniform averages dilutes signal from the most
  informative channels (likely margin and entropy, which are known
  baselines) with noise from channels that are redundant or even
  counter-informative on MC2.
- The **max-per-token** reduction of the scalar σ — i.e. the worst
  σ value encountered anywhere along the continuation — is directly
  useful as a ranking signal on the two hardest tasks.  This is
  consistent with the intuition that a single high-uncertainty token
  is often a stronger indictment of the continuation than the average
  uncertainty, and it is the one non-pre-registered finding we
  consider worth following up.

## Honest interpretation guide

If you read this file and are unsure what to write in a paper
abstract:

> **"On BitNet b1.58 2B-4T at n=500 per task, the v101 σ-stack's
> eight-channel *mean* abstention signal is statistically indistinguishable
> from entropy-alone abstention (all ΔAURCC within bootstrap CI);
> however, the *max-per-token σ* reduction outperforms entropy on
> arc_challenge (ΔAURCC = −0.020) and truthfulqa_mc2 (ΔAURCC = −0.044)
> at suggestive but sub-CI-significant levels.  At this scale no
> claim of 'σ-stack improves selective prediction' is warranted; a
> claim of 'per-token-max σ is a useful ranking signal' is measurable
> and consistent with the hypothesis that σ's *peak* matters more
> than its *mean*."**

## Reproducibility stamp

```
date:             2026-04-17
host:             Apple M3 · macOS 15.7.4 · Darwin 24.6.0 arm64
backend:          bitnet.cpp + Metal (llama.cpp ggml-metal)
checkpoint:       ggml-model-i2_s.gguf  sha256 prefix 4221b252fdd5fd25
bitnet.cpp SHA:   01eb415772c342d9f20dc42772f1583ae1e5b102
lm-eval SHA:      c1c4bea3777f73e188395264083adcf454913344
wall_clock:       ~1 h 30 min  (20:53 → 22:23 Helsinki time, UTC+3)
ll-calls:         7 591  (arc_easy 1 998 + arc_challenge 1 997 + truthfulqa_mc2 3 596)
raw outputs:      benchmarks/v103/results/
                    samples_<task>_sigma.jsonl    (σ sidecar, per-candidate)
                    lm_eval/<task>/<hash>/         (lm-eval JSONL + per-sample)
                    rc_summary.json                 (AURCC / AUACC / Cov@ CI95)
                    rc_curves.json                  (sparse RC curves, all 6 signals × 3 tasks)
reproduce:        bash benchmarks/v103/run_sigma_log.sh --limit 500
                  .venv-bitnet/bin/python benchmarks/v103/compute_rc_metrics.py
```

## Open (P follow-ups)

1. **n = 5 000 per task.**  At n = 500 the CIs are ~0.06 wide; the
   σ_max_token − entropy gap on truthfulqa_mc2 (ΔAURCC = −0.044) is
   within that noise.  A 10× larger pass (~15 h wall) would tighten
   the CI to ~0.019 and determine whether σ_max_token is *significantly*
   better than entropy.
2. **Learned confidence head over the σ vector.**  v103 only tests the
   uniform mean of eight channels.  A small (≤ 100-parameter) logistic
   head trained on one task and evaluated on the others would test
   whether the *channels individually* carry complementary information
   that uniform averaging destroys.  A positive result here would
   rehabilitate the σ-stack as a selective-prediction signal.
3. **Instruction-tuned 1.58-bit checkpoint.**  BitNet-b1.58-2B-4T is
   a pretrain base.  The entropy-alone insufficiency finding of
   arXiv:2603.21172 is reported on instruction-tuned LLMs; a DPO/SFT
   checkpoint of the same size class would make the comparison fair.
4. **Reliability-diagram ECE.**  AURCC is ranking; ECE is calibration.
   Computing the ECE of σ on the per-candidate loglikelihood
   predictions is orthogonal to v103 and is the natural next step.

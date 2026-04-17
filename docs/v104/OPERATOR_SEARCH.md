# v104 — σ-Operator Search

**Status.** Measured (tier M) on the n=500 v103 sidecar; a follow-up
n=5000 pass on the full test splits of the three MC tasks is running
/ has run on this host (see [RESULTS.md](RESULTS.md) for the current
numbers).  v104 has **no C kernel**; it is three Python analysers on
top of v103's persisted σ-sidecar JSONL.

## Why v104 exists

v103 reported two findings on BitNet b1.58 2B-4T at n=500 per task:

1. The standard `sigma_mean` aggregate is **indistinguishable from
   entropy-alone abstention** on all three MC tasks (null result).
2. A secondary, **not pre-registered**, observation: the *max per
   token* reduction of σ beats entropy on arc_challenge (ΔAURCC =
   −0.020) and truthfulqa_mc2 (ΔAURCC = −0.044), suggestive but
   sub-CI at n=500.

v104 asks the honest next question: **which** σ-aggregation operator
is best, and is any of them statistically significantly better than
entropy after appropriate multiple-comparison correction?  Before
spending more wall time on `v105 representation surgery` or any
other σ-gated downstream kernel, we want the operator selected
explicitly, not implicitly.

## Pre-registered hypotheses

All three are fixed **before** any post-hoc number is read from the
n=5000 sidecar:

- **H1** — σ_max_token's advantage on truthfulqa_mc2 survives at
  n=5000: the paired bootstrap ΔAURCC(σ_max_token − entropy) is
  negative and its upper 95 % confidence bound is < 0.
- **H2** — some individual σ-channel (channel 1 through 7; entropy is
  channel 0, the reference) beats entropy at Bonferroni-corrected
  α = 0.05 / 7 ≈ 0.0071 on at least one task.  If true, the σ-stack
  is **over-aggregated**: mean-over-channels dilutes informative
  single channels with destructive ones.
- **H3** — the best entropy × σ_second hybrid achieves its optimal
  ΔAURCC at an **interior** α ∈ (0, 1), with the Bonferroni-corrected
  CI excluding 0.  That would mean σ and entropy carry *independent*
  information — the strongest paper-ready finding.

If none of H1/H2/H3 holds, v104 reports a clean honest negative:
"σ-stack is not a better selective-prediction signal than entropy on
BitNet at this scale", and `docs/ROADMAP_TO_AGI.md` shifts the σ-stack
from "selection signal" to "diagnostic only" — the σ vector still
carries interpretable information about distributional shape, but
abstention / gating downstream uses the entropy channel.

## Pre-registered operator list (10 candidates + controls)

All operators act on one sidecar row (the full 8-channel σ-profile +
`sigma_mean` + `sigma_max_token`).  Pre-registered operators are the
ten listed in `benchmarks/v104/operators.py`, in order:

| # | name                      | what it computes                                                                |
|---:|---------------------------|----------------------------------------------------------------------------------|
| 1 | `sigma_mean`              | v103 default — mean of 8 channels over tokens (null baseline)                    |
| 2 | `sigma_max_token`         | v103 secondary — worst per-token σ over the continuation                          |
| 3 | `sigma_max_channel`       | max over the 8 token-averaged channels                                             |
| 4 | `sigma_topk_mean_k3`      | mean of the top-3 (most uncertain) channels                                       |
| 5 | `sigma_p95_token`         | 0.75 · σ_max_token + 0.25 · σ_mean  (proxy for P95 over tokens; v104.5 re-log nails the true P95) |
| 6 | `sigma_product`           | geometric mean of the 8 channels                                                   |
| 7 | `sigma_l2`                | ‖profile‖₂ / √8                                                                     |
| 8 | `sigma_max_of_max`        | max(σ_max_token, max(profile))                                                      |
| 9 | `sigma_entropy_hybrid`    | 0.5 · σ_max_token + 0.5 · entropy (motivated by arXiv:2603.21172)                |
| 10 | `sigma_per_channel_best` | deferred — picks the best single channel found by `channel_analysis.py`             |

The controls tested alongside (no Bonferroni slot) are `entropy`,
`margin`, `p_max`, `random`.

## Pre-registered multiple-comparison correction

We run 10 σ-candidates vs. the entropy reference per task, per
statistical test.  Bonferroni-corrected two-sided α:

- **operator search** (10 candidates): α = 0.05 / 10 = **0.005**
- **per-channel analysis** (7 channels vs. entropy channel):
  α = 0.05 / 7 ≈ **0.00714**
- **α-sweep** (11 α-points, 0.0 … 1.0): α = 0.05 / 11 ≈ **0.00455**

Paired bootstrap (2 000 resamples, seeded) on ΔAURCC = AURCC(op) −
AURCC(entropy).  Both metrics recomputed per resample on the *same*
resampled doc list — this removes shared doc-level variance and is
roughly sqrt(2)-tighter than two independent CIs.

## What "n=5000" actually means for these tasks

lm-eval's `--limit` caps at dataset size.  For the three MC tasks we
use, the actual document counts are:

| task             | max docs | ll-calls (≈) |
|------------------|---------:|-------------:|
| `truthfulqa_mc2` | 817      | ~5 700       |
| `arc_challenge`  | 1 172    | ~4 700       |
| `arc_easy`       | 2 376    | ~9 500       |

So "n=5000" is really "run the full validation / test split".  Total
wall ≈ 5–6 h on Apple M3 / Metal / bitnet.cpp i2_s (from the v103
rate of ~1.4 ll-calls / sec).

## Read order for RESULTS

1. [RESULTS.md](RESULTS.md) — per-task operator comparison tables,
   channel ranking, α-sweep.  Each table marks "significant?"
   column according to the Bonferroni-corrected α for **that** test
   family.
2. [INTERPRETATION.md](INTERPRETATION.md) — which of H1/H2/H3 held,
   and what that implies for v105 and for `docs/ROADMAP_TO_AGI.md`.

## Non-goals (deliberately **not** in v104)

- No C kernel changes.  v29 σ-channels are not redesigned in v104.
- No representation surgery (that is v105).
- No RLHF / DPO (that is deficiency #7).
- No attempt to *learn* a confidence predictor.  All operators are
  *fixed* closed-form reductions of the σ vector.  A learned head is
  a separate experiment we have not run.

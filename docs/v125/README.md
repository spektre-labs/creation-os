# v125 σ-DPO — preference optimization without a human annotator

> "RLHF as Opinion Laundering" (Rainio, 2025) argues that human
> preference annotation is how an annotator's σ becomes the policy's
> prior.  v125 closes that loophole: the preference signal *is* σ.

## Problem

The audit flagged v42 self-play as a scaffold and its bench as a
stub: "needs a working preference optimizer".  Classical DPO
(Rafailov et al., 2023) closes the RLHF loop by replacing the
reward model with a direct preference objective — but it still
needs labeled `(chosen, rejected)` pairs.  Where do the labels come
from?  Today: paid annotators.  That's the laundering step.

## σ-innovation: σ **is** the preference

v125 emits preference pairs from two rules, both driven by the
σ_profile:

- **Rule A — correction pair.** When the operator corrected a
  reply, the correction is chosen and the original reply is
  rejected.  One round-trip through a real user produces one
  ground-truth preference.
- **Rule B — σ pair.** Two interactions sharing a `context_key`,
  one with σ_product < τ_low (confident, low entropy) and the other
  with σ_product > τ_high (uncertain), pair into chosen (low-σ) /
  rejected (high-σ).  No human needed — the model's own coherence
  measurement against itself drives the gradient.

Mid-σ rows are skipped.  Low-σ rows without a high-σ partner on the
same key are unused this epoch.  Every pair is tamper-evident: the
`source` tag on `cos_v125_pair_t` distinguishes CORRECTION from
SIGMA so downstream training and audits can weight them separately.

## DPO loss kernel

Standard DPO (numerically stable):

```
δ = β · ((logπ(y_w|x) − logπ_ref(y_w|x)) −
         (logπ(y_l|x) − logπ_ref(y_l|x)))
L = −log σ(δ) = softplus(−δ)
```

β defaults to 0.1 (conservative, stays below the mode-collapse
cliff).  `softplus` is implemented branchlessly with overflow
guards so extreme log-probs do not NaN.

The self-test pins the analytical limits:

| scenario | expected L |
|---|---|
| δ = 0 | log 2 ≈ 0.6931 |
| δ = +7.9 (strong chosen) | softplus(−7.9) ≈ 3.7e-4 |
| δ = −7.9 (strong rejected) | softplus(+7.9) ≈ 7.9 |
| β = 0 (all params) | log 2 |
| batch mean over 3 rows with δ = 0.2 | softplus(−0.2) ≈ 0.598 |

## Mode-collapse detector

DPO's classical failure is degenerate confidence — the policy
funnels into a single template, σ distribution collapses to a delta
at 0.  v125 tracks this:

```
cos_v125_sigma_distribution(sigmas, n, &stats);
  // → mean, stddev, Shannon entropy (10-bin histogram)

cos_v125_check_mode_collapse(cfg, &before, &after) →
  MODE_COLLAPSE  iff
    stddev_after / stddev_before         < 0.40  OR
    entropy_after / entropy_before       < 0.60
```

The caller (v106 server in v125.1) rolls back the DPO adapter on
collapse, the same pattern v124 uses for baseline accuracy.

## What ships in v125.0

- `cos_v125_label_dataset` — σ-derived pair emission with counters.
- `cos_v125_dpo_loss` — numerically-stable softplus(−δ) kernel.
- `cos_v125_dpo_batch_loss` — mean over a batch.
- `cos_v125_sigma_distribution` + `cos_v125_check_mode_collapse`.
- CLI: `--self-test | --loss | --label-synthetic | --collapse-demo`.
- Merge-gate smoke: asserts the analytical loss limits, the 6-row
  synthetic-corpus label counters (1 correction + 1 σ-pair + 1 mid
  skipped + 1 unpaired), and the collapse/no-collapse verdicts on
  a uniform-vs-delta σ distribution.

## What lands in v125.1

- MLX LoRA DPO training loop consuming `cos_v125_pair_t*`.
- Adapter saved to `models/v125/dpo_sigma.safetensors`, stacked on
  top of the v124 continual-learning adapter.
- TruthfulQA MC2 before / after + σ-distribution before / after
  stored in a repro bundle; mode-collapse verdict joins `merge-gate`.

## Source

- Header    : [`src/v125/dpo.h`](../../src/v125/dpo.h)
- Impl      : [`src/v125/dpo.c`](../../src/v125/dpo.c)
- CLI       : [`src/v125/main.c`](../../src/v125/main.c)
- Smoke     : [`benchmarks/v125/check_v125_dpo_smoke.sh`](../../benchmarks/v125/check_v125_dpo_smoke.sh)

## Self-test

```
make check-v125
```

Covers DPO loss at all analytical limits, dataset labeling across
all three label sources and both skip reasons, Shannon entropy /
stddev computation against a known uniform distribution, and both
collapse / no-collapse verdicts.

## Claim discipline

v125 is a **Lab demo (C)** — the *loss math* is proven correct
today; the *weight update* is deferred to v125.1.  No claim in the
README says "v125 improves TruthfulQA by X %"; when v125.1 lands,
measured rows join the repro bundle.

## σ-stack layer

**σ-Governance + Training** — this is the first kernel that uses σ
as a **gradient direction**, not just as a gate or a filter.  It is
the direct technical answer to the "Opinion Laundering" paper: teach
coherence from the model's own σ, not from a vendor's σ.

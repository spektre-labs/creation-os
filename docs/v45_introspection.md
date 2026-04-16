# v45 — σ as introspection (lab)

**Evidence class:** deterministic **calibration-gap** math + **doubt-reward** scalar + **internal-probe placeholder** (`make check-v45`). **Not** a public multi-model TruthfulQA scatter archive until a harness exists.

## What ships

| Module | Role |
|--------|------|
| `src/v45/introspection.c` | `v45_measure_introspection_lab` — σ-derived “actual confidence” from logits implied by text (via `v42_scratch_logits_from_text`), synthetic self-report (or caller override), `calibration_gap`, resample-variance `meta_sigma` |
| `src/v45/doubt_reward.c` | `v45_doubt_reward` — RLVR-shaped scalar rewarding calibrated doubt vs overconfident errors |
| `src/v45/clap_sigma.c` | `v45_probe_internals_lab` — deterministic toy **σ_internal** stand-in until hidden-state hooks exist |

## Honest limits

- This is **not** Anthropic-style concept injection replay; it is a **portable contract** for gap + meta-σ numbers you can later wire to real self-reports and activations.
- “Gemini paradox” scatter is **N** until `benchmarks/v45/*.json` rows exist (`make bench-v45-paradox` is a stub).

## Verify

```bash
make check-v45
make bench-v45-paradox
```

## Working paper title (only when harness exists)

**“σ as Introspection: Calibrated Self-Knowledge in Language Models via Epistemic Uncertainty Decomposition”**

Thesis sentence: treat **calibration_gap** between verbalized confidence and σ-derived confidence as a public **introspection** statistic; combine with **K_eff = (1−σ)·K** to compare *useful* knowledge across model scales.

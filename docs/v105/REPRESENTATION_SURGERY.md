# v105 — σ-aggregator default-swap (representation surgery, phase 1)

**Scope.** One-line behavioural change, entirely inside
`src/v101/sigma_channels.c`: the scalar `sigma` field of
`cos_v101_sigma_t` now defaults to the **geometric mean** of the eight
σ-channels instead of the arithmetic mean.  No new forward passes, no
new benchmark runs, no API breakage.  This is the production-track
first step the v104 operator search earned the right to make.

## Why product, not mean

The v104 pre-registered operator search
(`docs/v104/RESULTS.md`, n = 4 365, paired bootstrap 2 000×) compared
ten post-hoc aggregators on BitNet b1.58 2B-4T across
`arc_easy` / `arc_challenge` / `truthfulqa_mc2`.  Taking AURCC as the
selective-prediction metric (lower is better, Bonferroni α = 0.005 for
ten comparisons):

| operator            | arc_easy ΔAURCC | arc_chall. ΔAURCC | truthfulqa ΔAURCC | verdict                       |
| ------------------- | --------------- | ----------------- | ----------------- | ----------------------------- |
| `sigma_mean` (old)  | +0.0017         | +0.0042           | +0.0089           | **null / slightly worse**     |
| `sigma_product`     | +0.0003         | **−0.0087**       | **−0.0114**       | **cross-task robust winner**  |
| `sigma_max_token`   | (n/a)           | (n/a)             | **−0.0447**       | hard-task specialist (H1)     |

`sigma_product` is the **only** operator that is Bonferroni-significant
on both hard tasks simultaneously **and** Δ-neutral (not worse) on
`arc_easy`.  The mechanism is visible in the per-channel analysis: the
`logit_std` channel is a statistically significant *anti*-signal on
both ARC tasks (p = 0.0005) — arithmetic mean propagates its damage,
geometric mean suppresses it via `log(1 + small) ≈ small`.

`sigma_max_token` is the harder-task specialist (H1 confirmed on
truthfulqa_mc2, Δ = −0.0447), but its behaviour on `arc_easy` was not
pre-registered and not measured at n = 5 000.  v105 therefore keeps
`max_token` available as an alternative aggregator (via
`s.sigma_max_token` at the callsite level) but does not flip it to
default.

## What actually changed

### `cos_v101_sigma_t` got two new fields

```170:176:src/v101/bridge.h
    float sigma;            /* default aggregator output (PRODUCT unless
                               overridden).  This is the value abstention
                               thresholds are compared against. */
    float sigma_arith_mean; /* arithmetic mean of the 8 channels; what the
                               `sigma` field was on pre-v105 builds. */
    float sigma_product;    /* geometric mean of the 8 channels, with an
                               epsilon floor of 1e-6 to avoid log(0). */
```

Every pre-v105 consumer of `s.sigma` still compiles unchanged; the
scalar it reads is now the geometric mean.  Consumers that need the
arithmetic mean explicitly (for backward comparison) use
`s.sigma_arith_mean`.  Consumers that want the geometric mean
explicitly (regardless of what the default is set to) use
`s.sigma_product`.

### Runtime aggregator selector

```37:56:src/v101/bridge.h
/* σ-aggregator choices.
 *
 * v104's pre-registered operator search (`docs/v104/RESULTS.md`) compared
 * ten post-hoc reductions of the 8-channel σ profile on BitNet b1.58 2B-4T
 * across arc_easy / arc_challenge / truthfulqa_mc2 (n = 4 365).  The
 * geometric mean (PRODUCT) is the only operator that beats entropy
 * Bonferroni-significantly on both hard tasks simultaneously, and is
 * Δ-neutral on arc_easy.  v105 flips the default aggregator from MEAN to
 * PRODUCT; MEAN remains accessible for backward compatibility and for
 * consumers that have calibrated thresholds against the arithmetic mean.
 */
typedef enum cos_v101_sigma_agg {
    COS_V101_AGG_MEAN    = 0,  /* arithmetic mean of 8 channels (pre-v105 default) */
    COS_V101_AGG_PRODUCT = 1,  /* geometric mean of 8 channels (v105+ default) */
} cos_v101_sigma_agg_t;
```

The default can be changed three ways:

1. **C API.** `cos_v101_set_default_aggregator(COS_V101_AGG_MEAN)`
   restores the pre-v105 scalar at any point; it returns the previous
   value so callers can save/restore.
2. **Environment variable.** `COS_V101_AGG=mean` or
   `COS_V101_AGG=product` at process start wins before any call to
   `cos_v101_sigma_from_logits` happens.  Unknown values fall back to
   `product` (v105 default).
3. **No override.** The default is `COS_V101_AGG_PRODUCT`.

### CLI JSON output got three extra fields

Every `/ll` (loglikelihood) response now contains, in addition to the
historical `sigma_mean`:

```json
{
  "sigma":            0.0032,
  "sigma_aggregator": "product",
  "sigma_mean":       0.0428,
  "sigma_product":    0.0032,
  "sigma_max_token":  0.6801,
  "sigma_profile":    [...8 floats...]
}
```

`sigma_mean` retains its pre-v105 semantic (per-continuation mean of
the arithmetic-mean channel scalar) for bit-exact backward
compatibility with the v102/v103/v104 sidecar analysers.  v106's
OpenAI-compatible HTTP server will surface `sigma` and the full
profile as its authoritative abstention inputs.

## What did NOT change

- None of the eight channel definitions.  Every channel is computed
  identically; only the scalar that reduces them is different.
- `cos_v101_sigma_from_logits` error semantics (`-1` for NULL /
  `n_vocab < 2`).
- `cos_v101_bridge_loglikelihood_ex` signature — the v105 additions
  are in a new parallel function `cos_v101_bridge_loglikelihood_v105`
  that also returns the per-continuation geometric-mean scalar.
- The v103 sidecar format — new C builds still write `sigma_mean` with
  the arithmetic-mean semantic the v104 analysers expect.
- `make merge-gate` — it was green pre-v105 and is green post-v105
  (31/31 on `check-v101`, everything else unchanged).

## Self-test additions

`src/v101/self_test.c` grew from 19 to 31 checks.  The new ones assert:

- uniform logits → both aggregators ≥ 0.7, default ≥ 0.7,
- peaked logits → both aggregators < 0.4, default < 0.4,
- product ≤ mean on values in [0, 1] (classical inequality),
- monotone peak-sharpness separately for each aggregator,
- aggregator-selector round-trip (`MEAN ↔ PRODUCT`) flips
  `s.sigma` to the matching explicit field,
- `sigma_product` computed from the post-hoc formula on the recorded
  channel values matches `s.sigma_product` within 1e-4 (float
  round-trip tolerance) — the direct regression against
  `benchmarks/v104/operators.py::op_sigma_product`.

## Calibration note for τ

Abstention thresholds calibrated against the arithmetic-mean scale
(v103 used τ ≈ 0.5–0.7) are **too loose** against the geometric-mean
scale, because `sigma_product` is strictly smaller than `sigma_mean`
on values in [0, 1].  A pre-v105 deployment using `τ = 0.6` will
abstain much less on a v105 build at the same τ.  Two choices are
supported:

- **Keep the old τ behaviour** — pass `COS_V101_AGG=mean` at process
  start (or call `cos_v101_set_default_aggregator(COS_V101_AGG_MEAN)`
  in the HTTP server's startup path).  v106 will expose this as a
  `[sigma] aggregator = "mean"` config option.
- **Recalibrate.** The v104 sidecars already contain the geometric
  mean per-example, so the AURCC vs coverage curve is measurable on
  the n = 4 365 fixture.  v106 ships `product` as default with
  `tau_abstain = 0.7` as a reasonable starting point; τ can be tuned
  with `benchmarks/v104/hybrid_sweep.py` or a bespoke sweep over the
  existing sidecar JSONL.

## Files touched

- `src/v101/bridge.h` — new enum, new fields, new accessor prototypes.
- `src/v101/sigma_channels.c` — arithmetic + geometric mean computed
  in one pass; env-var resolution; aggregator-name helper.
- `src/v101/bridge_real.c` — per-continuation mean accumulates both
  scalars; new `_v105` wrapper threads `out_sigma_product`.
- `src/v101/bridge_stub.c` — stub of the new `_v105` entry point.
- `src/v101/creation_os_v101.c` — `/ll` JSON includes `sigma`,
  `sigma_aggregator`, `sigma_mean`, `sigma_product` together.
- `src/v101/self_test.c` — 12 new assertions (31/31 total).
- `tests/v105/README.md` — test rationale.
- `docs/WHAT_IS_REAL.md`, `docs/ROADMAP_TO_AGI.md` — tier moves.

## What this does NOT claim

This layer does not prove `sigma_product` is "the right aggregator" in
any universal sense.  It claims, narrowly, what the n = 4 365 v104
evidence supports: on BitNet b1.58 2B-4T across three multiple-choice
tasks, `sigma_product` is the only pre-registered operator that is
Bonferroni-significantly non-worse than entropy on `arc_easy` and
Bonferroni-significantly better than entropy on the two hard tasks.
Other models, other tasks, other distribution shapes — open, see
v109.

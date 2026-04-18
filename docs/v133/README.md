# v133 — σ-Meta

**System-level metacognition: self-benchmark + meta-σ + auto-diagnose + regression detection.**

## Problem

The other 100+ kernels each measure their own σ.  But nothing, until v133, asks: *as a whole, is the system getting better, staying stable, or degrading?*  If the v124 continual-adapter is corrupting the base model, we want to know *this week*, not after users complain.

## σ-innovation

Five primitives in pure C:

### 1. Weekly snapshot history
Bounded ring.  Each snapshot: σ_product mean, per-channel σ means (8 channels, same axes as v122 red-team), interaction count, timestamp.

### 2. OLS trend slope
Linear regression across the history, returned in **σ units / week** for human readability.  Runs over either σ_product or any single channel.

### 3. Regression verdict
```
if slope_per_week  > τ_reg   → REGRESSION_DETECTED   (lifts v124 rollback)
if meta_sigma      > τ_meta  → CALIBRATION_DRIFT     (supersedes)
else                         → OK
```

### 4. Meta-σ  (σ of σ itself)
```
meta_sigma = stddev(σ_product) / mean(σ_product)       (coefficient of variation)
```
Captures **K(K) = K** — the holographic principle as engineering.  If meta-σ is high the σ values themselves are drifting, i.e. calibration is broken: you can no longer trust *anything* downstream that was keyed on σ.

### 5. Auto-diagnose
The highest per-channel σ in the latest snapshot maps to a concrete cause label:

| Channel | Cause |
|---|---|
| 0 | adapter_corrupted |
| 1 | kv_cache_degenerate |
| 2 | memory_polluted |
| 3 | tool_sigma_spike |
| 4 | plan_sigma_spike |
| 5 | vision_sigma_spike |
| 6 | red_team_fail |
| 7 | formal_violation |

v133.1 will wire each cause to its remedy (rollback, KV reset, memory prune, etc).

### Self-benchmark runner
Takes a caller-supplied `cos_v133_answer_fn(question, out, cap, σ, ctx)` that produces a response + σ per question; harness computes accuracy (case-insensitive substring match) and mean σ over a smoke-set.  Framework-free so it can run under any transport; v133.1 wires it to v106 `/v1/reason` as a weekly cron.

## Contract

```c
cos_v133_history_init      (&h)
cos_v133_history_append    (&h, &snapshot)
cos_v133_slope_per_week    (&h)
cos_v133_channel_slope_per_week(&h, channel)
cos_v133_meta_sigma        (&h)
cos_v133_regression_verdict(&h, τ_reg, τ_meta)     → verdict
cos_v133_diagnose          (&latest, spike_τ)      → cause
cos_v133_run_benchmark     (fn, ctx, qs, exps, n, &result)
cos_v133_health_compose    (&h, τ_reg, τ_meta, spike_τ, &health)
cos_v133_health_to_json    (&health, out, cap)
```

## What's measured by `check-v133`

- 8 stable weekly snapshots → `verdict = ok`, meta-σ < 0.05.
- 8 monotonically-rising snapshots (σ + 0.03/week) → `slope_per_week ≈ 0.030`, `verdict = regression_detected`.
- 8 noisy snapshots (alternating σ ≈ 0.10 / 0.85) → `meta_sigma ≈ 0.75`, `verdict = calibration_drift`.
- Diagnose `0.1×6 + 0.9 at channel 6` → `red_team_fail`.
- Diagnose `0.1, 0.85, 0.1, ...` → `kv_cache_degenerate`.

## What ships now vs v133.1

| v133.0 (this commit) | v133.1 (follow-up) |
|---|---|
| Pure-C snapshot history + OLS + meta-σ + diagnose + bench | Dashboard view on v108 web UI (`/dashboard`) |
| Composite `cos_v133_health_t` JSON | `GET /v1/meta/health` on v106 |
| Deterministic mock benchmark in merge-gate | Weekly cron with real 100-question smoke-set |
| Cause enum + labels | Remedy hooks: rollback, KV reset, memory prune |

## K(K)=K

This is the point of v133.  σ is the confidence of the model.  Meta-σ is the confidence of *σ itself*.  When the two diverge, we can no longer claim anything about the stack.  Keeping meta-σ under control across sessions is what makes the rest of Creation OS's "σ-governed" claim operational rather than rhetorical.

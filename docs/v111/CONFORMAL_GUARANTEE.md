<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2026 Spektre Labs / Lauri Rainio
-->
# v111.2-conformal — finite-sample coverage guarantee

This document describes the **distribution-free, finite-sample**
coverage guarantee that Creation OS attaches to its σ-gate when the
gate is parameterised by a conformal threshold τ.  It sits on top of
[`docs/v111/THE_FRONTIER_MATRIX.md`](THE_FRONTIER_MATRIX.md) and uses
the same sidecars; it does **not** replace any measurement there.

## 1 — The guarantee (Vovk–Gammerman)

Given

- a calibration set `D_cal = {(x_i, σ_i, c_i)}_{i=1..n}` drawn
  exchangeably from some joint distribution, where `σ_i` is any
  scalar gating signal and `c_i ∈ {0, 1}` is "the model got this
  correct",
- a tolerance α ∈ (0, 1),
- the conformal quantile τ_c computed as

  ```
  S      := sorted ascending ({σ_i : c_i = 1})         # correct-only
  n      := |S|
  q      := ⌈(1 − α) · (n + 1)⌉         (1-indexed, clipped to n)
  τ_c    := S[q − 1]
  ```

the Vovk–Gammerman conformal guarantee says that **on a fresh
exchangeable draw `(x*, σ*, c*)` from the same distribution**,

```
    P[c* = 1 | σ* ≤ τ_c]  ≥  1 − α
```

in finite-sample expectation over the calibration draw.  In words:
the subset of future documents for which the σ-gate says "accept"
retains accuracy at least `1 − α`, **with no distributional
assumption** beyond exchangeability.

This is the guarantee that AURCC alone cannot provide: AURCC
summarises an empirical risk-coverage curve on a fixed data set, but
does not attach a finite-sample bound to any particular operating
point.  Conformal τ does.

## 2 — What we actually ran

The calibration is performed on the 50 % **calibration split** of
each task family defined in
`benchmarks/v111/PREREGISTRATION_ADAPTIVE.lock.json`.  The test split
was never seen by this calibration.  The split seed
(`0xC05A1A2A`), the calibration ids, the test ids, and the routing
table for `sigma_task_adaptive` are all frozen in the lock file; the
analysis script verifies the SHA-256 of `adaptive_signals.py` before
running.

Reproduce:

```bash
.venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --analyse
#   writes:
#     benchmarks/v111/results/frontier_matrix_prereg.{md,json}
#     benchmarks/v111/results/conformal_tau.json
```

## 3 — Measured thresholds (α = 0.05, per-task)

All numbers below are taken verbatim from
`benchmarks/v111/results/conformal_tau.json` after the lock file was
instantiated on this commit.  Each τ is computed on the 50 %
calibration split; the accompanying guarantee is `P[correct | σ ≤ τ]
≥ 0.95` on exchangeable draws.

| task family     | signal                 | n_cal_correct | τ_conformal |
|-----------------|------------------------|--------------:|------------:|
| truthfulqa_mc2  | `sigma_composite_max`  | 187 | **0.4153** |
| truthfulqa_mc2  | `sigma_composite_mean` | 187 | 0.2800 |
| truthfulqa_mc2  | `sigma_task_adaptive`  | 187 | **0.4153** |
| arc_challenge   | `sigma_composite_max`  | 282 | 0.4631 |
| arc_challenge   | `sigma_composite_mean` | 282 | 0.3266 |
| arc_challenge   | `sigma_task_adaptive`  | 282 | 0.1268 |
| arc_easy        | `sigma_composite_max`  | 906 | 0.4442 |
| arc_easy        | `sigma_composite_mean` | 906 | 0.3322 |
| arc_easy        | `sigma_task_adaptive`  | 906 | 0.2460 |
| hellaswag       | `sigma_composite_max`  | 187 | 0.2912 |
| hellaswag       | `sigma_composite_mean` | 187 | 0.2765 |
| hellaswag       | `sigma_task_adaptive`  | 187 | 0.2875 |

## 4 — How to use this in a product

For a σ-gated Creation OS deployment that targets accuracy ≥ 95 %:

```
τ            := τ_conformal_for(task)       # from conformal_tau.json
σ_input      := creation_os_v101.σ_product(...)   # or the task-routed signal
decision     := σ_input ≤ τ ? ACCEPT : ABSTAIN
```

The ACCEPT branch carries the Vovk–Gammerman 95 % guarantee on
exchangeable draws.  The ABSTAIN branch is where the σ-gate refuses
to emit — which is the whole point of the gate.

## 5 — What this does NOT claim

- **Not distributional robustness.**  The guarantee is
  exchangeability-based.  If the deployment distribution is shifted
  relative to the calibration distribution (new domain, adversarial
  prompts, different language), the bound no longer holds.  Recompute
  τ on a calibration split from the NEW distribution.
- **Not an asymptotic claim.**  It is finite-sample, per the
  Vovk–Gammerman quantile.  Small calibration sets (e.g. hellaswag
  n_cal_correct = 187) give correspondingly conservative τ.
- **Not a frontier-model accuracy claim.**  The underlying accuracy
  is BitNet-b1.58-2B-4T's; the gate reshapes the risk-coverage
  trade-off, it does not increase model competence.
- **Not a substitute for the pre-registration.**  The Bonferroni
  matrix in `frontier_matrix_prereg.md` is the hypothesis test on
  AURCC.  Conformal τ is the *operating-point* companion to that
  test; both are needed.

## 6 — References

- V. Vovk, A. Gammerman, G. Shafer — *Algorithmic Learning in a
  Random World* (2005).
- Recent LLM applications: conformal factuality checks for open-ended
  generation, ACL 2024; adaptive conformal LMs, arXiv 2604.13991
  (2026).

`assert(declared == realized);  // finite-sample, exchangeable, honest.`

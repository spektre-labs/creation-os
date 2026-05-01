<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
# v105 — σ-aggregator default-swap tests

v105 changes the default σ-aggregator inside `cos_v101_sigma_from_logits`
from the **arithmetic mean** of the eight σ-channels to the **geometric
mean (a.k.a. σ_product)** — the cross-task-robust winner identified in
the v104 operator search (`docs/v104/RESULTS.md`).  The change is purely
in which scalar the `sigma` field of `cos_v101_sigma_t` points to; every
channel is still computed, every byte of the profile is still emitted,
and `sigma_arith_mean` / `sigma_product` are both always present so
downstream consumers that key on either one keep working.

## Test surfaces

1. **`make check-v101`** (`src/v101/self_test.c`) — 30 checks.  Besides
   the 19 pre-v105 channel-math checks, v105 adds:
   - uniform logits → both `sigma_arith_mean` and `sigma_product`
     ≥ 0.7 and monotone,
   - peaked logits → both < 0.4, plus `sigma_product ≤
     sigma_arith_mean` (geometric mean never exceeds arithmetic mean
     on values in [0, 1], a clean invariant to assert),
   - peak-sharpness monotonicity separately for both aggregators,
   - round-trip of `cos_v101_set_default_aggregator` (MEAN → PRODUCT →
     MEAN) with verification that `s.sigma` switches accordingly,
   - `cos_v101_aggregator_name` returns `"mean"` / `"product"`.
2. **Regression against v104 Python operator**
   (`tests/v105/test_regression.py`) — compares the geometric mean
   produced by the new C path with `benchmarks/v104/operators.py::
   op_sigma_product` on a small fixture of hand-rolled logits.  They
   must agree to 1e-4 float tolerance (the only source of difference is
   double → float round-trip of the individual channels).
3. **`make check-v103` and `make check-v104`** — both continue to run
   their synthetic oracle / random sanity on the same 500-doc fixture
   and must remain green.  This verifies the post-hoc analysers did
   not regress when the default aggregator flipped (they key on the
   `sigma_mean` field which now reports the arithmetic mean explicitly
   instead of the old `s.sigma` alias).

## Debug tips

- `COS_V101_AGG=mean` at process start reverts `s.sigma` to the
  arithmetic mean everywhere.  Useful when comparing against pre-v105
  recorded σ numbers or when a downstream consumer has τ calibrated
  against the old scale (arithmetic-mean abstention thresholds are
  generally larger than geometric-mean thresholds).
- The v105 CLI JSON output always includes both `sigma_mean` and
  `sigma_product` plus a short `sigma_aggregator` tag naming which one
  fed `sigma`.  Backends and dashboards should key on the explicit
  name and not assume `sigma` means any particular aggregator.

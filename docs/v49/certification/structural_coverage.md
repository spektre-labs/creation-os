# Structural coverage — MC/DC notes (v49 lab)

## Scope

This document focuses on the **decision logic** in `v49_sigma_gate_calibrated_epistemic` because it is a compact boolean structure suitable for MC/DC education.

## Decision (as implemented)

Pseudo-representation:

```text
ABSTAIN := (!isfinite(epistemic)) || (!isfinite(threshold)) || (epistemic > threshold)
```

## MC/DC-style tests (implemented)

These cases are exercised in `tests/v49/mcdc_tests.c`:

| Case | epistemic | threshold | Expected | Independent effect |
|---|---:|---:|---|---|
| A | `NaN` | `0.5` | `ABSTAIN` | non-finite epistemic forces abstain |
| B | `0.8` | `0.7` | `ABSTAIN` | comparison true forces abstain |
| C | `0.5` | `0.7` | `EMIT` | comparison false allows emit (finite path) |

> Full MC/DC for short-circuiting across **both** finiteness checks requires additional cases (finite/infinite threshold). Extend `mcdc_tests.c` when you tighten the gate.

## Coverage artifacts

Run:

```bash
make certify-coverage
```

Outputs (best-effort):

- `.build/v49-cov/*.gcda` / `*.gcno`
- `docs/v49/certification/coverage/*.gcov` (when `gcov` is available)
- optional HTML: `docs/v49/certification/coverage/html/` (when `lcov` + `genhtml` exist)

## DO-178C honesty clause

**100% MC/DC on the entire product** is not claimed here. This is a **kernel/gate starter pack** with a coverage driver.

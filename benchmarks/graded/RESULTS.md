# Graded benchmark — full metrics

**Generated (UTC):** 2026-04-26T05:23:28Z  
**Source CSV:** `benchmarks/graded/graded_results.csv`  

## Scope and honesty

- **AUROC / AURC / ECE** are standard *reporting* quantities on this run.
- **Split τ search** uses half the shuffled rows for threshold selection and half for reporting test accuracy; finite-sample **test** accuracy can fail the target — the table marks **OK/FAIL**.
- This is **not** a substitute for a full conformal prediction proof; exchangeability with future prompts is an **assumption**.

## AUROC / AURC

- **AUROC:** 0.8123
- **AURC:** 0.0425
- **Correct / incorrect:** 43 / 7

## Conformal-style split calibration

```text
=== CONFORMAL-STYLE SPLIT CALIBRATION (τ from cal, check test) ===
Calibration set: 25 prompts | Test set: 25 prompts

Disclaimer: this is an **empirical split** (shuffle seed 42, first half / second half). It is **not** a full distribution-free conformal guarantee without the usual CP assumptions and finite-sample correction; test accuracy can still fall below the target. Report ✓/✗ honestly.

Target mean accuracy on accepted (cal): ≥ 95% (α=0.05)
  τ = 0.2160
  Test: accuracy=0.0% FAIL | coverage=0% (0/25 accepted)

Target mean accuracy on accepted (cal): ≥ 90% (α=0.1)
  τ = 0.2160
  Test: accuracy=0.0% FAIL | coverage=0% (0/25 accepted)

Target mean accuracy on accepted (cal): ≥ 85% (α=0.15)
  τ = 0.2160
  Test: accuracy=0.0% FAIL | coverage=0% (0/25 accepted)

Target mean accuracy on accepted (cal): ≥ 80% (α=0.2)
  τ = 0.2160
  Test: accuracy=0.0% FAIL | coverage=0% (0/25 accepted)

Target mean accuracy on accepted (cal): ≥ 70% (α=0.3)
  τ = 0.2160
  Test: accuracy=0.0% FAIL | coverage=0% (0/25 accepted)

```

## ECE (1 − σ proxy)

```text
=== CALIBRATION (ECE) ===
Confidence proxy: 1 − σ_combined; bins = 5
   Bin     Conf      Acc      Gap     N
     2    0.287    0.733    0.447    15
     3    0.495    0.864    0.369    22
     4    0.656    1.000    0.344    13

ECE = 0.3857
  (0.0 = perfect calibration on this proxy)
  (<0.10 = often called well calibrated heuristically)
  (>0.20 = poorly calibrated on this proxy — report honestly)
```

## Definitive table

```text
=======================================================
CREATION OS σ-GATE: DEFINITIVE RESULTS (this CSV)
Model note: gemma3:4b (set COS_BITNET_CHAT_MODEL when running graded bench)
Prompts: 50
=======================================================
Metric                                        Value
-------------------------------------------------------
Accuracy (all answers)                       86.0%
Accuracy (σ-accepted only, non-ABSTAIN)          82.9%
Accuracy lift (answered − all)               -3.1%
Abstention rate                              18.0%
Wrong + low σ (σ<0.3, incorrect)                  0
Empirical 90% acc. threshold (τ, full set)          0.3180
Coverage at that τ (fraction of prompts)           6.0%
Accuracy at that τ (prefix)                 100.0%
=======================================================
```

## Files

- `benchmarks/graded/risk_coverage.csv`
- `benchmarks/graded/conformal_thresholds.csv`


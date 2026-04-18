# v187 σ-Calibration — continuous σ calibration

## Problem

σ is only as useful as its calibration. If σ=0.3 does not
mean "≈30 % chance of error" then σ is noise, and every
downstream gate (v138, v181, v182, v183) is built on sand.
Models drift; their σ drifts; without continuous calibration
the whole σ-governance stack rots silently.

## σ-innovation

v187 enforces calibration end-to-end:

```
holdout (500 Q&A) ─► replay through the σ stack
                 ─► bucket into 10 σ-bins
                 ─► Expected Calibration Error (ECE)
                 ─► temperature T :  σ_cal = σ^(1/T)
                 ─► per-domain T :  {math, code, history, general}
                 ─► recalibrate when ECE > τ_ece (= 0.05)
```

v187.0 ships a deterministic, weights-free fixture: 500
samples stratified across 10 error-rate bands (0.05 … 0.95).
Raw σ is overconfident (`σ_raw = p_err^{1.8}`) so the ECE has
something to fix. Temperature scaling is a closed-form
golden-section search over `T ∈ [0.3, 4.0]` globally and
per-domain. The test domain also adds a small shift so that
per-domain `T` differs from the global `T`.

## Merge-gate

`make check-v187` runs
`benchmarks/v187/check_v187_calibration_ece_below_005.sh`
and verifies:

- self-test
- 500 samples, 10 bins, 4 domains
- raw ECE ≥ 0.10 (starting miscalibration is real)
- calibrated ECE < 0.05
- every per-domain T is finite and > 0
- every per-domain ECE_calibrated ≤ ECE_raw
- ≥ 4 bins strictly improve after calibration
- Σ bin sizes = 500
- output byte-deterministic

## v187.0 (shipped) vs v187.1 (named)

| | v187.0 | v187.1 |
|-|-|-|
| Holdout | 500 synthetic samples | live worker rotation (v156 style) |
| Family | temperature only | + Platt + isotonic + Beta |
| Storage | in-process only | `models/v187/calibration_T.bin` |
| Trigger | manual run | wired into v159 heal + v181 audit log |
| UI | CLI JSON | Web-UI reliability curve per domain |

## Honest claims

- **Is:** a deterministic calibration kernel that drives ECE
  below the 0.05 σ-alarm threshold on a 500-sample holdout,
  per-domain and globally, via closed-form temperature
  scaling.
- **Is not:** a live calibration rotator. Live holdout
  rotation, the alternative recalibrator families and the
  v159 / v181 wiring ship in v187.1.

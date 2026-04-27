# TruthfulQA-200 benchmark results

**Run scope:** first **50** prompts of the 200-row stratified set (`grade.py run --limit 50`); full matrix: omit `--limit` (expect long wall clock). **Backend:** Ollama `gemma3:4b`; `cos chat --fast --no-cache`, pinned τ; `COS_ENGRAM_DISABLE=1` in harness.

**Generated (UTC):** 2026-04-27T12:14:43Z
**Commit:** `4066154101e18fdfc0d0685231560d38d59ec4bd`
**Hardware:** `Darwin Mac 24.6.0 Darwin Kernel Version 24.6.0: Wed Jan  7 21:18:30 PST 2026; root:xnu-11417.140.69.708.2~1/RELEASE_ARM64_T8122 arm64`
**Model:** `gemma3:4b`
**N prompts:** 50

| Metric | Value |
|--------|-------|
| AUROC (σ predicts incorrect; sklearn) | 0.8261 |
| Accuracy (non-ABSTAIN only) | 6.12% |
| Abstention rate | 2.00% |
| Wrong + confident (ACCEPT, σ<0.3, graded wrong) | 0 |
| ECE (10 bins, conf=1−σ) | 0.6080 |
| AURC (risk–coverage, σ desc.) | 0.9629 |

## Repro bundle

- `results.csv`: path `benchmarks/truthfulqa200/results.csv`
- `prompts.csv`: path `benchmarks/truthfulqa200/prompts.csv`
- `REPRO_CHECKSUMS.txt`: SHA-256 for `prompts.csv` + this `results.csv` (N=50 run).

## Notes

- AUROC requires both correct and incorrect in the non-ABSTAIN pool; otherwise NaN.
- This harness does **not** substitute for TruthfulQA leaderboard submission rules.

## Conformal guarantees (split calibration)

Random 50/50 split stratified by **correct** (0/1), seed=42. Rule: **ACCEPT** iff σ < τ (calibration set picks τ). This is an **empirical** split, not a full exchangeability proof — see Angelopoulos & Bates (2023); Gui et al. (2024) for references.

| Target acc (cal) | τ | Empirical acc (cal, σ<τ) | Coverage (cal) | Acc (test, σ<τ) | Coverage (test) |
|---:|---:|---:|---:|---:|---:|
| 80% | 0.3033 | 100.0% | 4.2% | 33.3% | 12.0% |
| 85% | 0.3033 | 100.0% | 4.2% | 33.3% | 12.0% |
| 90% | 0.3033 | 100.0% | 4.2% | 33.3% | 12.0% |
| 95% | 0.3033 | 100.0% | 4.2% | 33.3% | 12.0% |

*Figure skipped: install `matplotlib` to emit accuracy–coverage PNG.*


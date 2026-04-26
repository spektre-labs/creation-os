# TruthfulQA-200 benchmark results

> **Run class:** `cos chat` **stub** backend (not Ollama / llama.cpp). Metrics below validate the harness pipeline only; re-run with a real backend for model-facing numbers.

**Generated (UTC):** 2026-04-26T19:16:19Z
**Commit:** `724fbc6e99409d9a6c0f3aeb12b21072b77c883f`
**Hardware:** `Darwin Mac 24.6.0 Darwin Kernel Version 24.6.0: Wed Jan  7 21:18:30 PST 2026; root:xnu-11417.140.69.708.2~1/RELEASE_ARM64_T8122 arm64`
**Model:** `default`
**N prompts:** 200

| Metric | Value |
|--------|-------|
| AUROC (σ predicts incorrect; sklearn) | 0.5000 |
| Accuracy (non-ABSTAIN only) | 14.00% |
| Abstention rate | 0.00% |
| Wrong + confident (ACCEPT, σ<0.3, graded wrong) | 172 |
| ECE (10 bins, conf=1−σ) | 0.7800 |
| AURC (risk–coverage, σ desc.) | 0.8402 |

## Repro bundle

- `results.csv`: path `benchmarks/truthfulqa200/results.csv` (local; gitignored unless you add a `!` rule)
- `prompts.csv`: path `benchmarks/truthfulqa200/prompts.csv`
- `REPRO_CHECKSUMS.txt`: SHA-256 for `prompts.csv` and this stub `results.csv` (regenerate after a new run).

## Notes

- AUROC requires both correct and incorrect in the non-ABSTAIN pool; otherwise NaN.
- This harness does **not** substitute for TruthfulQA leaderboard submission rules.

## Conformal guarantees (split calibration)

Random 50/50 split stratified by **correct** (0/1), seed=42. Rule: **ACCEPT** iff σ < τ (calibration set picks τ). This is an **empirical** split, not a full exchangeability proof — see Angelopoulos & Bates (2023); Gui et al. (2024) for references.

| Target acc (cal) | τ | Empirical acc (cal, σ<τ) | Coverage (cal) | Acc (test, σ<τ) | Coverage (test) |
|---:|---:|---:|---:|---:|---:|
| 80% | — | — | — | — | — |
| 85% | — | — | — | — | — |
| 90% | — | — | — | — | — |
| 95% | — | — | — | — | — |

*Figure skipped: install `matplotlib` to emit accuracy–coverage PNG.*


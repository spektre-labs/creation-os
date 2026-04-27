# TruthfulQA-200 benchmark results

**Run (Wave 6):** N=**100** (first 100 of 200 prompts); Ollama **gemma3:4b**; `cos chat` **`--multi-sigma`** (full semantic σ), **`--no-cache`**, `τ_accept=0.3`, `τ_rethink=0.7`, `COS_ENGRAM_DISABLE=1`, `COS_SEMANTIC_SIGMA_PARALLEL=0`. Resumed after Cursor stop (append from row 81).

**Generated (UTC):** 2026-04-27T17:07:56Z
**Commit:** `0b85b90617b8f2a16babceea66880be5742f39fc`
**Hardware:** `Darwin Mac 24.6.0 Darwin Kernel Version 24.6.0: Wed Jan  7 21:18:30 PST 2026; root:xnu-11417.140.69.708.2~1/RELEASE_ARM64_T8122 arm64`
**Model:** `gemma3:4b`
**N prompts:** 100

| Metric | Value |
|--------|-------|
| AUROC (σ predicts incorrect; sklearn) | 0.5135 |
| Accuracy (non-ABSTAIN only) | 38.96% |
| Abstention rate | 23.00% |
| Wrong + confident (ACCEPT, σ<0.3, graded wrong) | 1 |
| ECE (10 bins, conf=1−σ) | 0.0600 |
| AURC (risk–coverage, σ desc.) | 0.5763 |

## Repro bundle

- `results.csv`: path `benchmarks/truthfulqa200/results.csv`
- `prompts.csv`: path `benchmarks/truthfulqa200/prompts.csv`
- `REPRO_CHECKSUMS.txt`: SHA-256 for `prompts.csv` + this N=100 `results.csv`.

## Notes

- Default `grade.py run` invokes `cos chat --multi-sigma` (full semantic σ); `--fast` is **opt-in** for quick smoke tests only (compressed σ).
- AUROC requires both correct and incorrect in the non-ABSTAIN pool; otherwise NaN.
- This harness does **not** substitute for TruthfulQA leaderboard submission rules.

## Conformal guarantees (split calibration)

Random 50/50 split stratified by **correct** (0/1), seed=42. Rule: **ACCEPT** iff σ < τ (calibration set picks τ). This is an **empirical** split, not a full exchangeability proof — see Angelopoulos & Bates (2023); Gui et al. (2024) for references.

| Target acc (cal) | τ | Empirical acc (cal, σ<τ) | Coverage (cal) | Acc (test, σ<τ) | Coverage (test) |
|---:|---:|---:|---:|---:|---:|
| 80% | 0.4013 | 100.0% | 2.6% | 0.0% | 5.1% |
| 85% | 0.4013 | 100.0% | 2.6% | 0.0% | 5.1% |
| 90% | 0.4013 | 100.0% | 2.6% | 0.0% | 5.1% |
| 95% | 0.4013 | 100.0% | 2.6% | 0.0% | 5.1% |

*Figure skipped: install `matplotlib` to emit accuracy–coverage PNG.*


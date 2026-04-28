# σ-Ablation v2 Results

*Diagnostic harness only — do not merge into flagship TruthfulQA headline claims without a repro bundle.*

## Best configurations (AUROC > 0.60)

| Rank | Model | Signal | n_samples | Temp | AUROC | σ_gap | Accuracy | Abstain% |
|---:|---|---|---:|---|---:|---:|---:|---:|
| 1 | gemma3_4b | `sigma_semantic` | 3 | `fixed_0.5` | 1.0000 | 0.5300 | 1.0000 | 0.5000 |
| 2 | gemma3_4b | `sigma_semantic` | 3 | `fixed_0.7` | 1.0000 | 0.5700 | 1.0000 | 0.5000 |
| 3 | gemma3_4b | `sigma_combined_semantic_0.5_0.5` | 3 | `legacy` | 1.0000 | 0.6000 | 1.0000 | 0.5000 |
| 4 | gemma3_4b | `sigma_logprob` | 3 | `legacy` | 1.0000 | 0.6000 | 1.0000 | 0.5000 |
| 5 | gemma3_4b | `sigma_semantic` | 3 | `legacy` | 1.0000 | 0.6000 | 1.0000 | 0.5000 |
| 6 | gemma3_4b | `sigma_semantic` | 3 | `mct_0.3_0.9` | 1.0000 | 0.4800 | 1.0000 | 0.5000 |
| 7 | gemma3_4b | `sigma_seu` | 3 | `mct_0.3_0.9` | 1.0000 | 0.4800 | 1.0000 | 0.5000 |

## MCT vs fixed temperature

| Model | Signal | Fixed 0.5 AUROC | Fixed 0.7 AUROC | MCT AUROC | Winner |
|---|---|---|---:|---:|---:|---|
| bitnet_2b_current | `sigma_logprob` | nan | nan | nan | — |
| bitnet_2b_current | `sigma_semantic` | nan | nan | nan | — |
| bitnet_2b_current | `sigma_seu` | nan | nan | nan | — |
| gemma3_4b | `sigma_logprob` | nan | nan | nan | — |
| gemma3_4b | `sigma_semantic` | 1.0000 | 1.0000 | 1.0000 | fixed_0.5 (1.0000) |
| gemma3_4b | `sigma_seu` | nan | nan | 1.0000 | MCT (1.0000) |

## SEU vs semantic entropy

| Model | n_samples | Temp | SE AUROC | SEU AUROC | Δ AUROC | Winner |
|---|---:|---|---:|---:|---:|---|
| gemma3_4b | 3 | `fixed_0.5` | 1.0000 | nan | nan | — |
| gemma3_4b | 3 | `fixed_0.7` | 1.0000 | nan | nan | — |
| gemma3_4b | 3 | `legacy` | 1.0000 | nan | nan | — |
| gemma3_4b | 3 | `mct_0.3_0.9` | 1.0000 | 1.0000 | 0.0000 | tie |

## Signal comparison (best AUROC per model, then averaged across models)

| Signal | Mean AUROC | Mean σ_gap | Mean Accuracy |
|---|---:|---:|---:|
| `sigma_combined_semantic_0.5_0.5` | 1.0000 | 0.6000 | 1.0000 |
| `sigma_cos_fast_scalar` | 0.0000 | -0.0400 | nan |
| `sigma_logprob` | 1.0000 | 0.6000 | 1.0000 |
| `sigma_semantic` | 1.0000 | 0.5300 | 1.0000 |
| `sigma_seu` | 1.0000 | 0.4800 | 1.0000 |

## Conclusions

- MCT improves AUROC (≥0.05): **NO** (Δ = 0.0000)
- SEU improves over SE (≥0.05): **NO** (Δ = 0.0000)
- Best AUROC achieved: 1.0000
- σ-gate viable (AUROC > 0.65 on some arm): **YES**
- Recommendation: At least one (model, n, signal, temp_strategy) arm exceeds 0.65 AUROC — worth deeper validation; diagnostic only until a repro bundle exists.

---

## Historical / raw sections

### Best overall (by AUROC)

```json
{
  "model": "gemma3_4b",
  "n_samples": 3,
  "temp_strategy": "legacy",
  "signal": "sigma_combined_semantic_0.5_0.5",
  "n_rows": 2,
  "auroc": 1.0,
  "auprc": 1.0,
  "ece": 0.2,
  "accuracy_accepted": 1.0,
  "coverage": 0.5,
  "abstain_rate": 0.5,
  "wrong_confident_count": 0,
  "mean_sigma_correct": 0.22,
  "mean_sigma_wrong": 0.82,
  "sigma_gap": 0.6,
  "viable_auroc_above_0p65": true,
  "failed_auroc_below_0p54": false
}
```

### Best signal / weights (parsed)

```json
{
  "best_signal": "sigma_combined_semantic_0.5_0.5",
  "best_signal_family": "combined_semantic",
  "best_temp_strategy": "legacy",
  "best_weight_semantic_logprob": [
    0.5,
    0.5
  ],
  "best_auroc": 1.0,
  "best_sigma_gap": 0.6
}
```

### Model AUROC deltas (best arm per model id)

```json
{
  "gemma3_4b_minus_bitnet_auroc": 1.0,
  "phi4_minus_bitnet_auroc": null
}
```

### σ_semantic near-chance (failed diagnostic) by setup

AUROC ∈ [0.48, 0.54] ⇒ mark `sigma_semantic` as **non-separating** for that (model, n); not a hidden failure.

```json
[]
```

### Conclusions (threshold rules)

- Model-size signal: best gemma3_4b AUROC exceeds bitnet_2b_current by ≥0.10 (small / local model limitation plausible).

## Structured conclusions (MCT / SEU / combined)

```json
{
  "mct_vs_fixed": {
    "mct_best_auroc": 1.0,
    "fixed_best_auroc": 1.0,
    "mct_improves": false,
    "mct_improves_ge_0p03": false,
    "mct_improves_ge_0p05": false,
    "delta_auroc": 0.0
  },
  "seu_vs_semantic": {
    "seu_best_auroc": 1.0,
    "semantic_best_auroc": 1.0,
    "seu_improves": false,
    "seu_improves_ge_0p03": false,
    "seu_improves_ge_0p05": false,
    "delta_auroc": 0.0
  },
  "combined_vs_single": {
    "combined_best_auroc": 1.0,
    "single_best_auroc": 1.0,
    "combined_improves": false
  },
  "best_overall": {
    "signal": "sigma_combined_semantic_0.5_0.5",
    "model": "gemma3_4b",
    "n_samples": 3,
    "temp_strategy": "legacy",
    "auroc": 1.0,
    "sigma_gap": 0.6
  },
  "auroc_above_0p65_any_arm": true,
  "auroc_above_0.65": true,
  "recommendation": "At least one (model, n, signal, temp_strategy) arm exceeds 0.65 AUROC \u2014 worth deeper validation; diagnostic only until a repro bundle exists."
}
```


## Per (model, n, temp_strategy, signal)

| model | n | temp_strategy | signal | AUROC | viable | failed | gap |
|---|---:|---|---|---:|---:|---:|---:|
| gemma3_4b | 3 | `fixed_0.5` | `sigma_semantic` | 1.0000 | True | False | 0.5300 |
| gemma3_4b | 3 | `fixed_0.7` | `sigma_semantic` | 1.0000 | True | False | 0.5700 |
| gemma3_4b | 3 | `legacy` | `sigma_combined_semantic_0.5_0.5` | 1.0000 | True | False | 0.6000 |
| gemma3_4b | 3 | `legacy` | `sigma_logprob` | 1.0000 | True | False | 0.6000 |
| gemma3_4b | 3 | `legacy` | `sigma_semantic` | 1.0000 | True | False | 0.6000 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_semantic` | 1.0000 | True | False | 0.4800 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_seu` | 1.0000 | True | False | 0.4800 |
| bitnet_2b_current | 3 | `cos_fast_scalar` | `sigma_cos_fast_scalar` | 0.0000 | False | True | -0.0400 |

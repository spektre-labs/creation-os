# σ-Ablation v2 Results

*Diagnostic harness only — do not merge into flagship TruthfulQA headline claims without a repro bundle.*

## Best configurations (AUROC > 0.60)

| Rank | Model | Signal | n_samples | Temp | AUROC | σ_gap | Accuracy | Abstain% |
|---:|---|---|---:|---|---:|---:|---:|---:|
| — | *(none above 0.60 in this run)* | | | | | | | |

## MCT vs fixed temperature

| Model | Signal | Fixed 0.5 AUROC | Fixed 0.7 AUROC | MCT AUROC | Winner |
|---|---|---|---:|---:|---:|---|
| gemma3_4b | `sigma_logprob` | nan | nan | nan | — |
| gemma3_4b | `sigma_semantic` | 0.4758 | 0.4773 | 0.5240 | MCT (0.5240) |
| gemma3_4b | `sigma_seu` | 0.5485 | 0.4585 | 0.4332 | fixed_0.5 (0.5485) |

## SEU vs semantic entropy

| Model | n_samples | Temp | SE AUROC | SEU AUROC | Δ AUROC | Winner |
|---|---:|---|---:|---:|---:|---|
| gemma3_4b | 3 | `fixed_0.5` | 0.4439 | 0.4395 | -0.0044 | SE |
| gemma3_4b | 3 | `fixed_0.7` | 0.4646 | 0.4585 | -0.0061 | SE |
| gemma3_4b | 3 | `mct_0.3_0.9` | 0.5240 | 0.4332 | -0.0908 | SE |
| gemma3_4b | 5 | `fixed_0.5` | 0.4758 | 0.5485 | 0.0728 | SEU |
| gemma3_4b | 5 | `fixed_0.7` | 0.4773 | 0.3666 | -0.1107 | SE |

## Signal comparison (best AUROC per model, then averaged across models)

| Signal | Mean AUROC | Mean σ_gap | Mean Accuracy |
|---|---:|---:|---:|
| `sigma_combined_semantic_1_0` | 0.5240 | 0.0253 | 0.4135 |
| `sigma_combined_seu_1_0` | 0.5521 | 0.0334 | 0.4471 |
| `sigma_semantic` | 0.5240 | 0.0253 | 0.4135 |
| `sigma_seu` | 0.5485 | 0.0257 | 0.4471 |

## Conclusions

- MCT improves AUROC (≥0.05): **NO** (Δ = -0.0281)
- SEU improves over SE (≥0.05): **NO** (Δ = 0.0245)
- Best AUROC achieved: 0.5521
- σ-gate viable (AUROC > 0.65 on some arm): **NO**
- Recommendation: No preset uplift vs chance in this summary — treat as boundary evidence; do not upgrade gate claims without stronger separation.

---

## Historical / raw sections

### Best overall (by AUROC)

```json
{
  "model": "gemma3_4b",
  "n_samples": 5,
  "temp_strategy": "fixed_0.5",
  "signal": "sigma_combined_seu_1_0",
  "n_rows": 200,
  "auroc": 0.552080154612959,
  "auprc": 0.5966220407568431,
  "ece": 0.5220014167445587,
  "accuracy_accepted": 0.4470588235294118,
  "coverage": 0.85,
  "abstain_rate": 0.15000000000000002,
  "wrong_confident_count": 94,
  "mean_sigma_correct": 0.11314617765360865,
  "mean_sigma_wrong": 0.1465122858639312,
  "sigma_gap": 0.03336610821032254,
  "viable_auroc_above_0p65": false,
  "failed_auroc_below_0p54": false
}
```

### Best signal / weights (parsed)

```json
{
  "best_signal": "sigma_combined_seu_1_0",
  "best_signal_family": "combined_seu",
  "best_temp_strategy": "fixed_0.5",
  "best_weight_semantic_logprob": [
    1.0,
    0.0
  ],
  "best_auroc": 0.552080154612959,
  "best_sigma_gap": 0.03336610821032254
}
```

### Model AUROC deltas (best arm per model id)

```json
{
  "gemma3_4b_minus_bitnet_auroc": null,
  "phi4_minus_bitnet_auroc": null
}
```

### σ_semantic near-chance (failed diagnostic) by setup

AUROC ∈ [0.48, 0.54] ⇒ mark `sigma_semantic` as **non-separating** for that (model, n); not a hidden failure.

```json
[
  {
    "model": "gemma3_4b",
    "n_samples": 3,
    "temp_strategy": "mct_0.3_0.9",
    "auroc": 0.5240170142130927,
    "sigma_semantic_failed_diagnostic": true
  }
]
```

### Conclusions (threshold rules)

- σ_semantic AUROC ∈ [0.48, 0.54] (mark as non-separating / failed diagnostic for that setup): gemma3_4b|n=3|0.524

## Structured conclusions (MCT / SEU / combined)

```json
{
  "mct_vs_fixed": {
    "mct_best_auroc": 0.5240170142130927,
    "fixed_best_auroc": 0.552080154612959,
    "mct_improves": false,
    "mct_improves_ge_0p03": false,
    "mct_improves_ge_0p05": false,
    "delta_auroc": -0.02806314039986635
  },
  "seu_vs_semantic": {
    "seu_best_auroc": 0.5485426929392447,
    "semantic_best_auroc": 0.5240170142130927,
    "seu_improves": false,
    "seu_improves_ge_0p03": false,
    "seu_improves_ge_0p05": false,
    "delta_auroc": 0.024525678726152078
  },
  "combined_vs_single": {
    "combined_best_auroc": 0.552080154612959,
    "single_best_auroc": 0.5485426929392447,
    "combined_improves": false
  },
  "best_overall": {
    "signal": "sigma_combined_seu_1_0",
    "model": "gemma3_4b",
    "n_samples": 5,
    "temp_strategy": "fixed_0.5",
    "auroc": 0.552080154612959,
    "sigma_gap": 0.03336610821032254
  },
  "auroc_above_0p65_any_arm": false,
  "auroc_above_0.65": false,
  "recommendation": "No preset uplift vs chance in this summary \u2014 treat as boundary evidence; do not upgrade gate claims without stronger separation."
}
```


## Per (model, n, temp_strategy, signal)

| model | n | temp_strategy | signal | AUROC | viable | failed | gap |
|---|---:|---|---|---:|---:|---:|---:|
| gemma3_4b | 5 | `fixed_0.5` | `sigma_combined_seu_1_0` | 0.5521 | False | False | 0.0334 |
| gemma3_4b | 5 | `fixed_0.5` | `sigma_seu` | 0.5485 | False | False | 0.0257 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_combined_semantic_1_0` | 0.5240 | False | True | 0.0253 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_semantic` | 0.5240 | False | True | 0.0253 |
| gemma3_4b | 5 | `fixed_0.7` | `sigma_combined_semantic_1_0` | 0.4773 | False | True | -0.0123 |
| gemma3_4b | 5 | `fixed_0.7` | `sigma_semantic` | 0.4773 | False | True | -0.0123 |
| gemma3_4b | 5 | `fixed_0.5` | `sigma_combined_semantic_1_0` | 0.4758 | False | True | -0.0143 |
| gemma3_4b | 5 | `fixed_0.5` | `sigma_semantic` | 0.4758 | False | True | -0.0143 |
| gemma3_4b | 3 | `fixed_0.7` | `sigma_combined_semantic_1_0` | 0.4646 | False | True | -0.0305 |
| gemma3_4b | 3 | `fixed_0.7` | `sigma_semantic` | 0.4646 | False | True | -0.0305 |
| gemma3_4b | 3 | `fixed_0.7` | `sigma_combined_seu_1_0` | 0.4585 | False | True | -0.0531 |
| gemma3_4b | 3 | `fixed_0.7` | `sigma_seu` | 0.4585 | False | True | -0.0531 |
| gemma3_4b | 3 | `fixed_0.5` | `sigma_combined_semantic_1_0` | 0.4439 | False | True | -0.0461 |
| gemma3_4b | 3 | `fixed_0.5` | `sigma_semantic` | 0.4439 | False | True | -0.0461 |
| gemma3_4b | 3 | `fixed_0.5` | `sigma_combined_seu_1_0` | 0.4395 | False | True | -0.0466 |
| gemma3_4b | 3 | `fixed_0.5` | `sigma_seu` | 0.4395 | False | True | -0.0466 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_combined_seu_1_0` | 0.4332 | False | True | -0.0833 |
| gemma3_4b | 3 | `mct_0.3_0.9` | `sigma_seu` | 0.4332 | False | True | -0.0833 |
| gemma3_4b | 5 | `fixed_0.7` | `sigma_combined_seu_1_0` | 0.3666 | False | True | -0.0657 |
| gemma3_4b | 5 | `fixed_0.7` | `sigma_seu` | 0.3666 | False | True | -0.0657 |

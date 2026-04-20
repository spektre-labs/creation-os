# v111.2 MMLU subject discovery (accuracy floor)

- Candidate source: `bitnet-paper-2504.12285 + hendrycks-2009.03300`
- Limit per subject: **100 questions**
- Accuracy floor for σ-analysis: **> 0.40** (random on 4-choice = 0.25)

> Floor is prior-registered in the script, not tuned on measured data.
> σ-gated selective prediction requires non-trivial base accuracy; a model at random has no uncertainty structure to exploit.

| task | n | acc | vs random | passed floor | rationale |
|---|---:|---:|---:|:---:|---|
| `mmlu_high_school_psychology` | 92 | 0.783 | ++0.533 | **yes** | easy: factual + large n |
| `mmlu_marketing` | 100 | 0.770 | ++0.520 | **yes** | easy: Hendrycks 2020 Table 3 easy-cluster |
| `mmlu_us_foreign_policy` | 63 | 0.762 | ++0.512 | **yes** | easy: Hendrycks 2020 easy-cluster |
| `mmlu_high_school_government_and_politics` | 55 | 0.709 | ++0.459 | **yes** | medium: factual, widely used in leaderboards |
| `mmlu_sociology` | 100 | 0.700 | ++0.450 | **yes** | easy: Hendrycks 2020 Table 3 easy-cluster |
| `mmlu_human_sexuality` | 94 | 0.660 | ++0.410 | **yes** | medium: 131-q factual set, 2B avg-above |
| `mmlu_nutrition` | 100 | 0.610 | ++0.360 | **yes** | easy: health facts, 2B paper avg-above |
| `mmlu_global_facts` | 64 | 0.344 | ++0.094 |  | easy: Hendrycks 2020 easy-cluster |
| `mmlu_abstract_algebra` | 100 | 0.330 | ++0.080 |  | hard-control: already measured 0.33 in v111.2 |
| `mmlu_high_school_mathematics` | 89 | 0.292 | ++0.042 |  | hard-control: should fall below 40% floor |

**σ-analysis eligible (acc > 0.40):** 7 / 10 subjects

- `mmlu_marketing`
- `mmlu_sociology`
- `mmlu_high_school_psychology`
- `mmlu_nutrition`
- `mmlu_us_foreign_policy`
- `mmlu_high_school_government_and_politics`
- `mmlu_human_sexuality`

### What happens next

Only the eligible subjects are passed into `benchmarks/v111/analyse_mmlu_subset.py` for the full σ-AURCC + Bonferroni matrix.  Subjects below the floor are honestly reported as *excluded because the base model is near-random*, not as σ-gate failures.

### Reproduce

```bash
.venv-bitnet/bin/python benchmarks/v111/mmlu_subject_discovery.py \
    --limit 100
.venv-bitnet/bin/python benchmarks/v111/analyse_mmlu_subset.py \
    --eligible-only   # reads sigma_analysis_eligible from discovery
```

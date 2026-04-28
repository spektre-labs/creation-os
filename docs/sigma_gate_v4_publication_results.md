# σ-gate v4: publication-style validation table

This page is a **template for reporting** measured numbers from this repository
only. Fill **TBD** cells after `bash run_holdout_pipeline.sh` (or the equivalent
`make sigma-full-validation`) and inspect the JSON artefacts listed below.

**Claim discipline:** cross-domain rows use different label definitions than
TruthfulQA CV (see `benchmarks/sigma_gate_eval/run_cross_domain.py` docstring).
Do not merge TruthfulQA CV AUROC with harness MMLU / ARC figures in one
headline sentence. External method AUROCs belong in
[`docs/sigma_gate_v4_comparison_table.md`](sigma_gate_v4_comparison_table.md)
with the caveats stated there.

**Artefacts**

- Holdout: `benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json`
- Cross-domain: `benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json`
- In-distribution CV (reference bundle): `benchmarks/sigma_gate_lsd/results_full/manifest.json`

## Training (reference)

| Parameter | Value |
|-----------|-------|
| Method | LSD contrastive hidden-state probe |
| Training data | 800 pairs (TruthfulQA + GPT-2 synthetic negatives; see adapt script) |
| Model | GPT-2 (per manifest `hf_model`) |
| Truth encoder | per manifest `truth_encoder_name` |
| Epochs | per manifest `epochs` |

## Evaluation

| Benchmark | Type | AUROC (wrong vs σ) | N | Notes |
|-----------|------|--------------------|---|--------|
| TruthfulQA (5-fold CV) | In-distribution | **0.9428** (manifest) | 200-pair protocol | From `results_full/manifest.json` when present |
| TruthfulQA (holdout) | Held-out prompts | **0.9821** | 57 | `holdout_summary.json` (semantic + PRISM) |
| TriviaQA (subset) | Cross-task smoke | **0.9596** | 100 | Greedy GPT-2 + semantic vs aliases (`run_cross_domain.py`) |
| HaluEval (`qa_samples`, generative) | Cross-task smoke | **0.3828** | 100 | Model-generated answers; HF `hallucination` + cosine vs `answer` |

## Comparison (literature-facing)

Use [`docs/sigma_gate_v4_comparison_table.md`](sigma_gate_v4_comparison_table.md)
for externally cited method families; do not present unrelated benchmarks as
direct numeric comparisons without matching task and protocol.

## Key properties (implementation)

- Single forward pass for σ scoring (no multi-sample detector)
- 12-byte packed measurement (`SigmaGate.pack_measurement`)
- Open source: [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)

Foundation paper for the contrastive geometry line: [arXiv:2510.04933](https://arxiv.org/abs/2510.04933).

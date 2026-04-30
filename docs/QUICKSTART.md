
 Creation OS — quickstart (σ-gate)

Canonical repository: [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os).

## Score a `(prompt, response)` with the LSD probe (Python)

From the repository root, with `PYTHONPATH` pointing at `python/`:

```python
from cos.sigma_gate import SigmaGate
gate = SigmaGate("benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl")
sigma, decision = gate(model, tokenizer, prompt, response)
```

`decision` is `ACCEPT`, `RETHINK`, or `ABSTAIN`. The bundled pickle is trained on a **GPT-2** hidden-state layout; other HF checkpoints need a new probe (see `benchmarks/sigma_gate_lsd/adapt_lsd.py`).

## MCP (one config block)

Run the stdio server (requires `pip install 'mcp[cli]'` in your venv):

```bash
export PYTHONPATH=python
export SIGMA_PROBE_PATH=benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl
python3 -m cos.mcp_sigma_server
```

Gateway-style example (paths must be absolute on your machine): `configs/mcp/bifrost_sigma_gate.example.yaml`. Full notes: [`docs/MCP_SIGMA.md`](MCP_SIGMA.md).

## Representative lab AUROCs (LSD, wrong vs σ)

These numbers come from checked-in JSON summaries under `benchmarks/sigma_gate_eval/` (semantic MiniLM labeling, threshold **0.45**; see `docs/CLAIM_DISCIPLINE.md` before citing externally).

| Setting | AUROC (wrong vs σ) |
|---------|---------------------|
| TruthfulQA holdout (GPT-2 generator) | **0.982** (`results_holdout/holdout_summary.json`) |
| TriviaQA smoke (GPT-2 generator, cross-task) | **0.960** (`results_cross_domain/cross_domain_summary.json`) |

Cross-task rows are **not** comparable to TruthfulQA CV as a single headline metric.

## Gemma + HIDE (training-free, separate harness)

To score **google/gemma-2-2b-it** greedy completions with `SigmaHIDE(backend="lab")` on the first *N* holdout prompts (default 30), run `benchmarks/sigma_gate_scaling/run_gemma_eval.py`. Outputs: `benchmarks/sigma_gate_scaling/results_gemma/gemma_hide_summary.json`. Set **`HF_TOKEN`** or **`HUGGING_FACE_HUB_TOKEN`** in the environment if the Hub requires auth — **never** commit tokens.

## License

`SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only` (per-file headers in new Python sources). See repository `LICENSE`.

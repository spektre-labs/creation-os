# σ eval pipeline — runbook and report stub

<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
-->

## Security

- Export tokens in the shell only; do not commit `.env` or log lines that echo secrets.

## Detached overnight run (survives Cursor disconnect)

```bash
export HF_TOKEN='…'
export HUGGINGFACE_HUB_TOKEN="$HF_TOKEN"
mkdir -p logs
nohup bash scripts/sigma_eval_pipeline_nightly.sh >> logs/nightly_console.log 2>&1 &
echo $! > logs/nightly_nohup.pid
```

Progress: `tail -f logs/pipeline_*/pipeline.log` (open the newest `logs/pipeline_*` directory). Per-step logs: `streaming.log`, `router.log`, `gemma.log`, etc. Checkpoint: `checkpoint.txt`. JSON snapshots are copied into that directory when present.

## Hardware profile (example)

- Apple Silicon, 8 GB unified memory: prefer `--limit 10` for Gemma; MPS uses **float32** in `run_gemma_eval.py`; CPU uses **bfloat16**.

## Commands (run from repository root)

Set `PYTHONPATH=python` or rely on each script’s `sys.path` insert.

| Step | Command | Notes |
|------|---------|--------|
| 1 Streaming | `PYTHONPATH=python MPLBACKEND=Agg python3 benchmarks/sigma_gate_eval/run_streaming_eval.py --holdout-limit 12 --max-new-tokens 80` | Sliding-window halt on **normalized entropy**; default `--taus 0.95,0.98`. |
| 2 Router | `PYTHONPATH=python MPLBACKEND=Agg python3 benchmarks/sigma_gate_eval/run_router_eval.py --holdout-limit 20 --max-new-tokens 40` | Default bands `0.3 / 0.6 / 0.85`. GPT-2 entropy precheck often sits **~0.35–0.4** → mostly VERIFY (ABSTAIN no longer stuck at 100%). For more FAST on GPT-2 smoke, sweep e.g. `--tau-fast 0.42`. |
| 3 Gemma + HIDE | `HF_TOKEN=… PYTHONPATH=python python3 benchmarks/sigma_gate_scaling/run_gemma_eval.py --limit 10` | **Gemma2ForCausalLM** only; no GPT-2 fallback. MPS path avoids mandatory `accelerate`. |
| 4 HaluEval oracle | `PYTHONPATH=python python3 benchmarks/sigma_gate_eval/run_halueval_oracle_fix.py --limit 50` | Oracle pairs + AUROC monotonicity fix; needs LSD pickle + datasets. |
| 5 HIDE sweep | `PYTHONPATH=python MPLBACKEND=Agg python3 benchmarks/sigma_gate_eval/run_hide_eval.py` | Optional layers / limits per script `--help`. |
| 6 Calibration | `PYTHONPATH=python python3 benchmarks/sigma_gate_eval/run_calibration_eval.py` | As implemented in-tree. |
| 7 Cost | `PYTHONPATH=python python3 benchmarks/sigma_gate_eval/run_cost_eval.py` | Illustrative savings math — see CLAIM_DISCIPLINE. |
| 8 Cascade | `PYTHONPATH=python python3 benchmarks/sigma_gate_eval/run_cascade_eval.py` | |

## Latest smoke (local machine, short limits — fill full runs separately)

| Area | Metric | Value | Notes |
|------|--------|------:|-------|
| Streaming | halt_rate @ τ=0.95 | 0% | `--holdout-limit 4 --max-new-tokens 40`; mean ~40 tok/row |
| Router | ABSTAIN | 0% | `--holdout-limit 8`; all VERIFY with default bands on GPT-2 entropy |
| Router | FAST | 0% | σ_pre ~0.35–0.40; use `--tau-fast` sweep for more FAST on this probe |

## Targets (lab goals, not merge-gate promises)

Fill in **measured** columns after each run from the JSON under `benchmarks/sigma_gate_eval/results_*` / `results_gemma/`. Do not copy historical headline numbers without attaching the same repro bundle (git SHA, `uname`, raw JSON).

| Area | Metric | Target | Measured (fill in) |
|------|--------|--------|--------------------|
| Streaming | halt_rate | &lt; 40% | |
| Streaming | mean tokens / row | &gt; 20 | |
| Router | ABSTAIN rate | &lt; 15% | |
| Router | FAST rate | &gt; 50% | |
| Gemma + HIDE | AUROC wrong vs σ | &gt; 0.85 | |
| HIDE | AUROC | &gt; 0.80 | |
| Calibration | ECE | &lt; 0.05 | |
| HaluEval oracle | AUROC | &gt; 0.80 | |

## Kernel invariant

- **`python/cos/sigma_gate.h`** (12-byte `sigma_state_t`) is **not** modified by these harnesses; Python mirrors **`sigma_gate_core`**.

## License

- Code and maintainer docs: **LicenseRef-SCSL-1.0 OR AGPL-3.0-only** (see repository `LICENSE`).

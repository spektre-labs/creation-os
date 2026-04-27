# σ AUROC Rescue: Model Size, Sample Count, and Signal Composition Ablation

**Class:** diagnostic lab harness — **not** a substitute for the TruthfulQA-200 merge-gate or README headline numbers.

## What this answers

When TruthfulQA-style AUROC sits near chance (~0.5), this tree asks **why**:

1. **Model size** — does a larger Ollama model lift AUROC vs. a local `cos --fast` scalar?
2. **Sample count** — does increasing *N* i.i.d. completions at fixed temperature *T* lift separation?
3. **Signal composition** — does logprob-only, semantic-only, or a weighted blend discriminate better?

## How it works

- **Input:** `benchmarks/truthfulqa200/prompts.csv` (default first 100 rows; `--limit N`; `--full` uses `full_dataset_limit` from config, where **0** means the entire CSV).
- **Runner:** `run_sigma_ablation.py` calls **Ollama OpenAI-compatible HTTP** for *N* sequential samples at **fixed `temperature=0.7`**. This is **not** identical to the in-process `cos chat` mixed-temperature shadow (0.1 / 0.7 / 1.5); it isolates *sample count* and *T* for diagnosis. **Logprob σ** is computed per completion and **averaged across the N runs** when `top_logprobs` are present.
- **Semantic σ:** pairwise mean Jaccard on `first_sentence` snippets → `1 − mean_sim` with the same high/low similarity clamps as `cos_chat.c` `chat_multi_shadow` for the 3-sample case.
- **Logprob σ:** mean per-token **normalized entropy** from `logprobs.content[].top_logprobs` when the server returns them; otherwise **logprob-only and any combined row with non-zero logprob weight are skipped for that prompt** (no silent fallback to semantic-only in those arms).
- **Combined σ:** `w_sem·σ_sem + w_lp·σ_lp` (clipped to `[0,1]`). Weights are enumerated in `sigma_ablation_config.json`.
- **Optional `bitnet_2b_current`:** one `./cos chat --fast` scalar per prompt (pipeline σ); skipped if `./cos` is missing.

## Commands

Requires a running **Ollama** on `127.0.0.1:11434` for the default `make check-sigma-ablation` path (HTTP models). **CI / offline:** `SKIP_SIGMA_ABLATION_OLLAMA=1 make check-sigma-ablation` runs `py_compile` plus `analyze_sigma_ablation.py` on `fixtures/tiny_detail.jsonl` only.

```bash
SKIP_SIGMA_ABLATION_OLLAMA=1 make check-sigma-ablation   # no Ollama
make check-sigma-ablation    # Ollama reachability + tiny quick run + analyze
make sigma-ablation          # full matrix (long wall clock)
make sigma-ablation-analyze  # re-run analyze from existing jsonl
```

Or directly:

```bash
python3 benchmarks/sigma_ablation/run_sigma_ablation.py --check
python3 benchmarks/sigma_ablation/run_sigma_ablation.py --quick   # 4 prompts, gemma only
python3 benchmarks/sigma_ablation/run_sigma_ablation.py --limit 100 --models gemma3_4b
python3 benchmarks/sigma_ablation/run_sigma_ablation.py --full    # prompt count from config (0 = all rows)
python3 benchmarks/sigma_ablation/analyze_sigma_ablation.py
python3 benchmarks/sigma_ablation/analyze_sigma_ablation.py --plots   # sigma_roc.png + sigma_hist_correct_wrong.png (needs matplotlib)
```

## Outputs (under `results/`)

| File | Purpose |
|------|---------|
| `sigma_ablation_detail.jsonl` | One JSON object per (prompt × model × *N* × signal arm). |
| `sigma_ablation_summary.json` | Best AUROC arms, **best_signal_report**, model deltas, **σ_semantic near-chance flags**, threshold **conclusions**. |
| `sigma_ablation_table.md` | Human-readable table + parsed best weights. |

**Metrics (grouped):** AUROC, AUPRC, ECE, accuracy among accepted, coverage, abstain rate, **wrong_confident_count** (wrong answer and `sigma < 0.3`), mean σ correct / wrong, `sigma_gap`.

Optional ROC / histogram PNGs: pass **`--plots`** to `analyze_sigma_ablation.py` (requires **matplotlib**; best-arm only, ≥4 rows).

### Detail JSONL row shape (required keys)

Each line is one JSON object, for example:

```json
{"id":"q_0001","model":"gemma3_4b","n_samples":8,"temperature":0.7,"signal":"sigma_combined_0.5_0.5","sigma":0.42,"correct":true,"accepted":true,"answer":"…","reference":"…","sigma_logprob":0.35,"sigma_semantic":0.49,"weight_semantic":0.5,"weight_logprob":0.5}
```

## Interpretation rule

**AUROC ≈ 0.51 is a diagnosis, not a catastrophe.** If σ does not separate, the gate must not claim separation. If separation only appears with a larger model, more samples, or a different composition, record that as a **boundary condition**, not a silent fix.

Core principle: **do not defend σ — force it to prove itself.**

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

## Overnight crash-proof run

The ablation **does not depend on Cursor** once started with **`nohup`** (or **`launch_ablation_detached.sh`**, which uses **`setsid`** when available for a new session without a controlling tty).

**Important:** **`ollama serve`** started only inside Cursor’s integrated terminal may still exit when Cursor quits. For a run that must survive Cursor, start **Ollama** in **Terminal.app**, **tmux**, **ssh**, or **launchd** — not only inside the IDE.

Use **`benchmarks/sigma_ablation/launch_ablation_detached.sh`** for a single canonical driver (refuses duplicate launch if `ablation_master.pid` is alive). Or use **`make sigma-ablation-launch-detached`** from the repo root.

To remove stray **`cos chat --fast`** / extra **`run_sigma_ablation.py`** after a crash (keeps the process tree for `logs/ablation_master.pid`):

```bash
make sigma-ablation-prune-orphans
# or: bash benchmarks/sigma_ablation/prune_sigma_ablation_orphans.sh
```

The canonical driver is **`benchmarks/sigma_ablation/run_ablation_overnight.sh`** (repo-root detection — no hard-coded `~/creation-os`).

### 1. Ollama

```bash
ollama list
# Expect gemma3:4b (or adjust sigma_ablation_config.json)
ollama pull gemma3:4b   # if missing
```

Ensure the server is up **outside Cursor-only lifecycle** if you need the job to outlive the IDE (`ollama serve` in another terminal session or a service).

### 2a. Recommended: detached launcher (from repository root)

```bash
cd "$(git rev-parse --show-toplevel)"
chmod +x benchmarks/sigma_ablation/launch_ablation_detached.sh benchmarks/sigma_ablation/run_ablation_overnight.sh
bash benchmarks/sigma_ablation/launch_ablation_detached.sh
```

### 2b. Manual: nohup (from repository root)

```bash
cd "$(git rev-parse --show-toplevel)"
chmod +x benchmarks/sigma_ablation/run_ablation_overnight.sh
mkdir -p benchmarks/sigma_ablation/logs
nohup bash benchmarks/sigma_ablation/run_ablation_overnight.sh \
  > benchmarks/sigma_ablation/logs/nohup_ablation.log 2>&1 &
echo $! > benchmarks/sigma_ablation/logs/ablation_master.pid
echo "master PID: $(cat benchmarks/sigma_ablation/logs/ablation_master.pid)"
```

- **`nohup`** — ignores SIGHUP when the terminal closes.  
- **`setsid`** (launcher path) — new session without a controlling tty (stronger than `nohup` alone when available).  
- **`ablation_master.pid`** — PID of the outer `bash` wrapper (same process group as the driver after exec).  
- **Timestamped log** — `benchmarks/sigma_ablation/logs/ablation_YYYYMMDD_HHMMSS.log` receives full stdout/stderr from the driver (everything after Ollama preflight).  
- **`set -euo pipefail`** — fail fast on errors instead of hanging silently.

### 3. Git behavior (defaults are conservative)

| Variable | Default | Meaning |
|----------|---------|---------|
| `SIGMA_ABLATION_GIT_COMMIT` | `1` | Stage and commit `sigma_ablation_summary.json`, `sigma_ablation_table.md`, optional PNGs. |
| `SIGMA_ABLATION_COMMIT_DETAIL` | `0` | Set to `1` to also `git add -f` **`sigma_ablation_detail.jsonl`** (can be very large). |
| `SIGMA_ABLATION_GIT_PUSH` | `0` | Set to `1` to **`git push origin HEAD`** after a successful commit (off by default so `main` is not pushed accidentally). |

### 4. Monitor

```bash
# Wrapper still running?
ps -p "$(cat benchmarks/sigma_ablation/logs/ablation_master.pid)" && echo RUNNING || echo DONE

# Live wrapper log (often short: the driver redirects to the timestamped log after start)
tail -f benchmarks/sigma_ablation/logs/nohup_ablation.log

# Full transcript (recommended)
ls -t benchmarks/sigma_ablation/logs/ablation_*.log 2>/dev/null | head -1 | xargs tail -f

# Progress (detail lines)
wc -l benchmarks/sigma_ablation/results/sigma_ablation_detail.jsonl
```

### 5. Morning checks

```bash
sed -n '1,120p' benchmarks/sigma_ablation/results/sigma_ablation_table.md
python3 -m json.tool < benchmarks/sigma_ablation/results/sigma_ablation_summary.json | head -80
```

### Caveats

- **Ollama lifecycle** — if `ollama serve` was started only inside Cursor and Cursor exits, the server may stop; HTTP phases then fail until Ollama is restarted (prefer a host-level Ollama process).  
- **Sleep / power** — if the machine sleeps, the run stops. Disable sleep for AC power for overnight jobs.  
- **Wall clock** — full matrix over all prompts and models can take many hours; scale is configuration-dependent.

## Interpretation rule

**AUROC ≈ 0.51 is a diagnosis, not a catastrophe.** If σ does not separate, the gate must not claim separation. If separation only appears with a larger model, more samples, or a different composition, record that as a **boundary condition**, not a silent fix.

Core principle: **do not defend σ — force it to prove itself.**

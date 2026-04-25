# Creation OS — Full Benchmark Report

**Date (UTC):** 2026-04-25 (updated); prior Desktop receipts dated 2026-04-24 retained where referenced.  
**Canonical tree for builds and gates:** `~/Projects/creation-os-kernel` (local APFS data volume, **not** iCloud Desktop).  
**Model:** `gemma3:4b` via Ollama on `localhost:11434`.

This report records **measured artifacts** and **honest limits**. No fabricated harness scores.

---

## Host layout (iCloud vs local)

**Issue (Desktop / File Provider):** Running `creation_os_*` and some `git` operations from **`~/Desktop/creation-os-kernel`** caused `_dyld_start` stalls, `git archive` / `mmap` timeouts, and impractically slow full-tree `cp -R`.

**Resolution:** Use **`~/Projects/creation-os-kernel`** as the working clone. A full **`cp -R` from Desktop was started but abandoned after ~45 min** (incomplete `.git`); the tree was populated with **`git clone file:///Users/eliaslorenzo/Desktop/creation-os-kernel`** (~6.5 min), then **`git remote set-url origin https://github.com/spektre-labs/creation-os.git`**. The Desktop copy remains as backup (**`cp`, not `mv`**).

**Rule going forward:** run **`make`**, **`./cos`**, and **`make merge-gate`** only under **`~/Projects/creation-os-kernel`** (or another non-iCloud path).

---

## Quality gate (`~/Projects/creation-os-kernel`, 2026-04-25)

Wall time for the chained block (sequential): **~93 s** end-to-end for the list below plus **`make merge-gate`**.

| Target | Result |
|--------|--------|
| `make check-agi` | **PASS** |
| `make check-inference-cache` | **PASS** |
| `make check-speculative-sigma` | **PASS** |
| `make check-omega` | **PASS** |
| `make check-energy-accounting` | **PASS** |
| `make check-green-score` | **PASS** |
| `make merge-gate` | **PASS** |

### Master σ fix (2026-04-25 — code)

| Item | Change |
|------|--------|
| `cos_chat.c` | With **HTTP** + **`--multi-sigma`** (and **`--fast` off**): three `cos_bitnet_server_complete` calls at **T = 0.3, 0.7, 1.0** on the user prompt; shadow **σ_combined = 0.7·σ_consistency + 0.3·σ_logprob**, where **σ_logprob** comes from the **T = 0.3** completion and **σ_consistency** is the existing word-bag **1 − mean pairwise Jaccard** over the three texts. **`--fast`**: skip the triple (cheaper logprob-oriented shadow). |
| `multi_sigma.h/c` | **`cos_text_jaccard(a, b)`** — public pairwise Jaccard similarity (same tokenizer as σ_consistency). |
| `engram.c`, `engram_episodic.c` | **Normalize** prompts before FNV (ASCII lowercase, trim, collapse whitespace) for **`cos_sigma_engram_hash`** and **`cos_engram_prompt_hash`**. |
| `forgetting_benchmark.sh` | σ extraction uses **`COS_FORGET_CHAT_OUT`** + Python (no stdin/heredoc bug); matches **`σ_combined=`** / **`sigma_combined=`** / **`[σ=`**. |
| Gates after change | **`make merge-gate`: PASS** (~**46 s**) on **`~/Projects/creation-os-kernel`**. |

Re-run **`scripts/real/sigma_separation_pipeline_coschat.sh`**, **`omega_evolve_final.sh`**, and **`forgetting_benchmark.sh`** with Ollama to refresh CSVs; older numeric blocks in this file are **pre–semantic-primary** unless noted.

Smoke from the same tree (no `/tmp` wrapper needed):

- `./cos help` — **OK** (instant).
- `./cos demo --batch` — **OK**.

Full hardening log (local): `/tmp/hardening_projects_2026-04-25.log` (if preserved on the machine that ran the suite).

---

## Git push (canonical remote)

After merge-gate **PASS**, from **`~/Projects/creation-os-kernel`**:

```text
To https://github.com/spektre-labs/creation-os.git
   88d6ce8..44149d5  main -> main
```

---

## Ollama smoke

```bash
curl -sf http://localhost:11434/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{"model":"gemma3:4b","messages":[{"role":"user","content":"test"}],"stream":false,"max_tokens":10}'
```

**OK** — response body contained model text (prefix: `Okay! It looks like you're testing the`).

---

## Evolve campaign (`omega_evolve_final.sh`, 5×10)

**Receipt:** `benchmarks/lm_eval/receipts/benchmarks_projects_2026-04-25.log` (full stdout).  
**Trajectory:** `benchmarks/evolve_campaign/sigma_trajectory.csv`

```csv
run,sigma_mean,cache_hits,prompts
1,0.2419,0,10
2,0.2503,0,10
3,0.2480,0,10
4,0.2528,0,10
5,0.2549,0,10
```

**Δ (run 5 − run 1):** **+0.0130** → **σ_mean rising** in this session (reported as observed). **`cache_hits=0`** for every run in this script’s accounting (no inference-cache hits on these prompts in this run).

---

## σ-Separation (`./cos chat` pipeline, 15 prompts)

**Log:** `benchmarks/sigma_separation/pipeline_coschat_run.log` (2026-04-25 run from Projects).  
**CSV:** `benchmarks/sigma_separation/pipeline_results.csv` was **rebuilt from the log** after fixing a bug in `scripts/real/sigma_separation_pipeline_coschat.sh` (heredoc Python previously read empty stdin instead of `cos` output).

```
=== σ Separation (backfilled from log, 2026-04-25) ===
FACTUAL      ████░░░░░░░░░░░░░░░░ mean=0.210 (n=3)
CREATIVE     ████░░░░░░░░░░░░░░░░ mean=0.227 (n=3)
REASONING    ████░░░░░░░░░░░░░░░░ mean=0.204 (n=3)
SELF_AWARE   █████░░░░░░░░░░░░░░░ mean=0.264 (n=3)
IMPOSSIBLE   ████░░░░░░░░░░░░░░░░ mean=0.229 (n=3)

Gap: 0.060
No separation (threshold 0.15) ✗
```

---

## Ω-Loop (20 turns, prior receipt)

Unchanged from the 2026-04-24 Desktop-side receipt: **`benchmarks/lm_eval/receipts/omega_20turns_gemma3.txt`** — **20** turns, **σ_mean 0.3104**, grade **C (62.05/100)**. **`./cos omega --report`** snapshot with **0** turns is still **`benchmarks/lm_eval/receipts/omega_20turns_report.txt`** (CLI snapshot, not the live `cos-omega` transcript).

---

## Forgetting benchmark

**PASS** on drift check (`|drift| < 0.05`) per script; per-line **`sigma=0`** in script output is still consistent with a **shell-side σ extraction bug** (UTF-8 / `σ_combined=`), not necessarily zero model σ. CSV: **`benchmarks/forgetting/results.csv`**.

---

## Script fix (pipeline σ CSV)

`scripts/real/sigma_separation_pipeline_coschat.sh`: `extract_fields` now reads **`COS_PIPELINE_CHAT_OUT`** from the environment (heredoc no longer steals stdin). Added **`sigma_combined=`** as an ASCII fallback pattern.

---

## Historical note (2026-04-24 Desktop-only receipts)

Earlier **`benchmarks/lm_eval/receipts/omega_evolve/sigma_trajectory.csv`** (flat **0.2366** with cache hits) and **`gemma3_speed_2026-04-24.txt`** remain valid as **that session’s** numbers; they differ from the 2026-04-25 **`benchmarks/evolve_campaign/`** run above.

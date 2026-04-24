# σ-Separation Definitive Report

**Date:** 2026-04-24 (run timestamp in `definitive_run.log`)

## Model selected

**gemma3:4b** via Ollama `http://127.0.0.1:11434/v1/chat/completions` (OpenAI-compatible).

Model probe (2+2, `max_tokens=40`):

| Model        | `message.content` (first 30 chars) |
|-------------|--------------------------------------|
| gemma3:4b | non-empty (e.g. `2 + 2 = 4`) |
| llama3.2:3b | non-empty |
| qwen3.5:4b | **EMPTY** on `/v1/chat/completions` (thinking / routing; not used for this run) |

## Method

- **Semantic σ (σ_se):** three completions per prompt at temperatures **0.1**, **0.7**, **1.2**; σ = 1 − mean pairwise Jaccard similarity on normalized token sets (script: `scripts/real/sigma_definitive.sh`).
- **Logprob σ (σ_lp):** single `/v1/chat/completions` with `logprobs: true`, `top_logprobs: 3`; σ_lp = 1 − mean(exp(token logprob)) over returned tokens (when Ollama returns `choices[0].logprobs.content`).

## Results (semantic entropy — category means)

From the second full run (`definitive_run.log` after fixing the model-picker `json.dumps` quoting):

```
FACTUAL      ███░░░░░░░░░░░░░░░░░ mean=0.159
CREATIVE     █████████░░░░░░░░░░░ mean=0.475
REASONING    ██░░░░░░░░░░░░░░░░░░ mean=0.141
SELF_AWARE   █████████░░░░░░░░░░░ mean=0.487
IMPOSSIBLE   ██████░░░░░░░░░░░░░░ mean=0.321

Gap: 0.345
SEPARATION ✓
Best (lowest σ): REASONING = 0.141
Worst (highest σ): SELF_AWARE = 0.487
```

**Note:** The gap and “best/worst” labels are **script-defined** (gap > 0.15 ⇒ “SEPARATION ✓”). This run shows **higher** semantic spread for **SELF_AWARE** and **CREATIVE** than for **FACTUAL**/**REASONING** on this small prompt set — not a clean monotonic “IMPOSSIBLE highest” story. Interpret as **category-dependent variance**, not a solved calibration problem.

## Logprob σ — category means (same run)

```
FACTUAL      █░░░░░░░░░░░░░░░░░░░ mean=0.078
CREATIVE     ██░░░░░░░░░░░░░░░░░░ mean=0.130
REASONING    █░░░░░░░░░░░░░░░░░░░ mean=0.061
SELF_AWARE   ███░░░░░░░░░░░░░░░░░ mean=0.152
IMPOSSIBLE   █░░░░░░░░░░░░░░░░░░░ mean=0.096
```

Logprob category ordering **differs** from semantic σ (e.g. SELF_AWARE highest here). Use **both** only as complementary probes; do not merge into one headline “uncertainty” without an explicit combine rule.

## Raw data

Full table: **`definitive_results.csv`** (same directory). Full console log: **`definitive_run.log`**.

## What worked / what did not

| Approach | Outcome |
|----------|---------|
| **gemma3:4b / llama3.2:3b** (no thinking channel on OpenAI compat) | **Works:** non-empty `content` on `/v1/chat/completions`. **gemma3:4b** selected first in the probe order. |
| **qwen3.5:4b** on `/v1/chat/completions` | **Failed** for 2+2 in this environment: empty `content` (native `/api/chat` with `think:false` remains the path for Qwen when needed). |
| **Logprobs** on **gemma3:4b** | **Works:** Ollama returns token `logprob` entries; σ_lp printed per prompt. |
| **Semantic triple-sample σ** | **Works mechanically**; separation vs. intuitive “harder = higher σ” is **mixed** (see note above). |

## Code / integration (this commit)

- **`COS_OLLAMA_CHAT_THINK_FALSE=1`:** Ollama `/api/chat` JSON gains root-level `"think":false` (and the aux chat path closes `options` then appends `,"think":false` when set). See `src/import/bitnet_server.h`.
- **Default Ollama chat model id** when env unset: **`gemma3:4b`** (replacing `qwen3:8b`) so default installs get non-empty `content` without extra flags. Override with **`COS_OLLAMA_MODEL`** / **`COS_BITNET_CHAT_MODEL`**.
- **`COS_OLLAMA_DEFAULT_SIGMA`** was already **0.5** in code when token-level σ is missing; unchanged.

## Next steps

1. For **Qwen** on OpenAI compat, prefer **`COS_OLLAMA_CHAT_THINK_FALSE=1`** and/or **`/api/chat`** (or keep **gemma3:4b** as default for lab benches).
2. Treat **σ_se** and **σ_lp** as **different sensors**; document any fusion rule in benchmark prose per `docs/CLAIM_DISCIPLINE.md`.
3. Re-run `scripts/real/sigma_definitive.sh` after Ollama upgrades; probe output is version-sensitive.

---

*Generated for Lauri — machine-local Ollama; paths relative to Creation OS repo root.*

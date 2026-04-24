# σ pipeline wiring report (cos chat + Ollama)

**Date:** 2026-04-24 (authoring in-repo; re-run scripts after `ollama serve` for fresh numbers.)

## Code changes (summary)

| Area | Change |
|------|--------|
| `bitnet_server.c` | Ollama backend uses **`POST /v1/chat/completions`** with `logprobs`, `top_logprobs` (≤3), `max_tokens`, `temperature`; optional `top_k` from `COS_TOP_K`. |
| `bitnet_server.c` | Aggregate **`bns_sigma_from_logprobs_agg`**: `0.4·(1−mean p)+0.3·(1−exp min_lp)+0.3·(fraction of tokens with logprob<−3)`; `n==0` → **0.5**. |
| `bitnet_server.c` | Default OpenAI `model` when **`COS_BITNET_CHAT_MODEL` unset**: **`gemma3:4b`** if `COS_INFERENCE_BACKEND=ollama` **or** (`COS_BITNET_SERVER_EXTERNAL=1` and HTTP port **11434**); else **`bitnet`**. |
| `stub_gen.c` | HTTP chat path uses **`top_logprobs=3`**; exports **`cos_cli_use_bitnet_http()`**. |
| `cos_chat.c` | **`--semantic-sigma`**: two extra **`cos_bitnet_server_complete`** calls at **T=0.5** and **T=1.0** (with codex system prompt when present); **σ_semantic** = `cos_multi_sigma_consistency` on three texts; blends into **`σ_combined`** when `--multi-sigma` and HTTP backend (`0.5/0.3/0.2` vs `0.7/0.3` fallbacks per spec). |
| `cos_chat.c` | Legacy BSC semantic path: only if **`COS_CHAT_BSC_SEMANTIC_SIGMA=1`**. |

## Automated checks (this workspace)

| Step | Result |
|------|--------|
| `make check-bitnet-server-parse` | **OK** (10 JSON cases; expectations updated for new σ aggregate). |
| `make check-agi` | **OK** |
| `make merge-gate` | **OK** |
| `./cos help` | **OK** (head smoke) |

## Local smoke (`./cos chat` without healthy Ollama)

One run with `COS_BITNET_SERVER=1`, `COS_BITNET_SERVER_EXTERNAL=1`, `COS_BITNET_SERVER_PORT=11434`, `COS_BITNET_CHAT_MODEL=gemma3:4b` **timed out** on local inference and **escalated** to the stub cloud path (`backend=stub`, `route=ESCALATE`). The multi-σ line still printed, e.g. **`σ_combined=0.238`** with **`k=3`** when `--semantic-sigma` was not used in that snippet.

**For Lauri:** with **`ollama serve`** and **`gemma3:4b`** pulled, re-run:

```bash
export COS_BITNET_SERVER=1 COS_BITNET_SERVER_EXTERNAL=1
export COS_BITNET_SERVER_PORT=11434 COS_BITNET_CHAT_MODEL=gemma3:4b
export COS_ENGRAM_DISABLE=1
bash scripts/real/sigma_quick_verify.sh
# optional triple blend:
COS_SIGMA_QUICK_SEMANTIC=1 bash scripts/real/sigma_quick_verify.sh
bash scripts/real/sigma_separation_coschat.sh
```

Outputs: `benchmarks/sigma_separation/coschat_pipeline.csv` and `coschat_pipeline_run.log`.

## Ω-loop / evolve campaign

- **`scripts/real/omega_evolve_campaign.sh`** defaults **`COS_BITNET_SERVER_PORT`** to **11434** and sets **`COS_BITNET_CHAT_MODEL=gemma3:4b`** for each inner `cos chat` invocation.
- **`./cos-omega --turns 10`** was **not** re-run here (needs long-lived Ollama + policy keys as applicable). Run locally after model is up.

## Honesty

- **No fabricated** EASY/HARD σ thresholds in this file until `sigma_quick_verify.sh` is re-run against a **healthy** Ollama `gemma3:4b` endpoint.
- **Throughput / harness** claims stay governed by `docs/CLAIM_DISCIPLINE.md`; this report is **integration wiring + CI** only.

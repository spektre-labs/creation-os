# v44 — σ-native inference proxy (lab)

**Evidence class:** loopback HTTP + **stub** engine logits (`creation_os_proxy`); **not** a production vLLM/SGLang sidecar until a logits / logprobs bridge is archived.

## What ships

| Piece | Role |
|------|------|
| `src/v44/sigma_proxy.c` | Stub “engine” → per-token Dirichlet σ → aggregate → `decode_sigma_syndrome()` → execute abstain / fallback / cite / … |
| `src/v44/api_server.c` | `POST /v1/chat/completions` JSON with extra `choices[].sigma` fields; `stream:true` emits **demo** SSE lines (not engine-faithful streaming yet) |
| `src/v44/sigma_stream.c` | SSE `data:` JSON formatters (token + compact σ, alert line) |
| `src/v44/v44_ttc.c` | If first pass is `ACTION_CITE`, one **retry** with `[TTC]` prefix (handoff story to v41 budget forcing when a real engine callback exists) |
| `config/proxy.yaml` | Example routing / thresholds / ports — **intent**; YAML parser not in-tree |

## Verify

```bash
make check-v44
make bench-v44-overhead   # stub script; exits 0
```

## Honest limits

- No claim of **<5% overhead** until `benchmarks/v44/*.json` exists from a pinned harness.
- Mid-stream **retraction** is a **format + policy placeholder** in SSE demo mode, not a guaranteed tokenizer-safe rewind against a real engine stream.

## Working paper title (only when harness exists)

**“σ-Native Inference Proxies: Uncertainty-Gated OpenAI-Shaped Serving Across Engines”**

Thesis sentence: treat throughput-first engines as **fast generators**; put a **σ gate** in front so clients see calibrated actions (`emit`, `abstain`, `fallback`, …) and optional per-token σ telemetry.

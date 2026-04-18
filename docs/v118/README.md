# v118 — σ-Vision: Multimodal Input with σ-Gated Projection

BitNet 2B is text-only. v118 adds **image input** through a
separate vision encoder plus a fixed projection into BitNet's token
embedding space — the same representation-surgery approach used by
v104. No training is required: the projection matrix is computed
offline from 200 COCO caption pairs.

## The problem v118 solves

LLaVA, MiniGPT-4, and the rest of the "vision encoder + projection"
family have no way to say *"I'm uncertain about this image."* They
project, they concatenate, they generate — and when the image is
out-of-distribution (a line drawing, a chart, an abstract stain),
the caption comes out confident and wrong.

v118 measures σ on the **projection step itself**. If the projection
histogram entropy is high (the image doesn't look like anything the
encoder has a strong opinion about) the server abstains with

    "abstained": true,
    "projection_channel": "embedding_entropy"

and the downstream text generation is skipped. That's the minimum
contract: no silent confabulation.

## Architecture (target)

```
image bytes
   │
   ▼
SigLIP vision encoder (400M GGUF)   →   768-d image embedding
   │
   ▼
Projection matrix P ∈ ℝ^{768×2048}  →   2048-d token-space vector
   │                                        │
   ▼                                        ▼
σ estimator (projection entropy)       BitNet 2B as "first token"
   │
   ▼
σ-gate: abstain if σ > τ_vision
```

## What ships in v118.0

- **Request parser** for OpenAI `image_url` messages (`data:` URLs are
  base64-decoded in-process; `http(s):` URLs are parsed but *not*
  fetched — the caller handles HTTP).
- **Placeholder encoder + projection**: deterministic pseudo-random
  projection modulated by the image byte histogram. This gives a
  shape-correct 2048-d vector and a sensible σ signal without
  requiring SigLIP weights.
- **σ estimator**: normalised byte-histogram entropy. Flat
  histograms → high σ; peaky histograms → low σ. This is an honest
  out-of-distribution proxy until the real encoder lands.
- **σ-gate** at `τ_vision = 0.55` (default, configurable per request).
- **JSON response contract**:

  ```json
  {
    "sigma_product": 0.23,
    "tau_vision":    0.55,
    "abstained":     false,
    "embedding_dim": 2048,
    "projection_channel": "embedding_entropy_ok",
    "content_hash":  "a4b7f5ff",
    "preview":       [0.16, −0.02, …]
  }
  ```

## What lands in v118.1

- SigLIP 400M encoder loaded via `llama.cpp` (GGUF).
- Learned projection matrix `P` computed from COCO caption pairs.
- Wire-up to `POST /v1/chat/completions` so `{"type":"image_url"}`
  messages flow through the σ-gate and, on success, prepend the
  projected embedding as a BitNet pseudo-token.
- v108 web UI: drag-and-drop image onto the chat input.
- v114 swarm: a vision specialist routed by σ alongside text
  specialists.

## Source

- [`src/v118/vision.h`](../../src/v118/vision.h)
- [`src/v118/vision.c`](../../src/v118/vision.c) — base64, projection
  placeholder, σ estimator, JSON
- [`src/v118/main.c`](../../src/v118/main.c) — CLI (`--parse-body`,
  `--project`, `--self-test`)
- [`benchmarks/v118/check_v118_vision_smoke.sh`](../../benchmarks/v118/check_v118_vision_smoke.sh)

## Self-test

```bash
make check-v118
```

Runs:

1. `creation_os_v118_vision --self-test` — data URL round-trip,
   projection finiteness, uniform-byte input forcing abstain,
   peaky-histogram input passing the gate, JSON shape.
2. `check_v118_vision_smoke.sh` — end-to-end through the CLI:
   parse, project, abstain on uniform bytes, pass on peaky bytes,
   required JSON fields present.

## Claim discipline

- v118.0 is **the interface surface**: request parsing, projection
  contract, σ-gate, JSON shape. All of these are measured by tests.
- The **placeholder encoder** is not SigLIP. Its σ is a byte-
  histogram heuristic. It is useful for the contract and for OOD
  guards, but it is not a caption-quality signal.
- Caption-quality comparisons with LLaVA / MiniGPT-4 / Qwen-VL
  arrive with v118.1 when real weights are loaded. We do not claim
  those numbers today.
- The σ-gate genuinely prevents silent confabulation on uniform-byte
  inputs in v118.0 — the self-test proves it — but that is a narrow
  claim, not a SOTA claim.

## σ-stack layer

v118 lives at the **Generation** layer, alongside v101, as a
secondary input modality. The σ it produces feeds the same
v105 aggregator and v108 UI as every other channel.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_

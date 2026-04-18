# v106 — σ-Server (OpenAI-compatible HTTP layer)

## Why

The v101–v104 work established that the σ-stack is a real selective-
prediction signal (`sigma_product` beats entropy Bonferroni-
significantly on two of three MC tasks at n = 4 365).  v105 swapped
the scalar default from arithmetic mean to geometric mean.  None of
that matters to a developer who wants to run a local LLM with
σ-governance — they need a **server** that speaks the OpenAI
protocol, because that is what every local-LLM client on the planet
speaks.  v106 is that server.

## Surfaces

| method | path                     | purpose                                                   |
| ------ | ------------------------ | --------------------------------------------------------- |
| GET    | `/health`                | orchestrator liveness probe, returns `ok\n`               |
| GET    | `/v1/models`             | lists the loaded model with a `sigma_aggregator` hint     |
| GET    | `/v1/sigma-profile`      | last σ-channel snapshot (8 channels + aggregator + abstain) |
| POST   | `/v1/chat/completions`   | OpenAI chat — non-streaming JSON or SSE (`stream:true`)   |
| POST   | `/v1/completions`        | OpenAI text completion                                    |
| GET    | `/`                      | serves `web/index.html` (reserved for v108)               |
| GET    | other                    | static files from `web/` (simple mime-type dispatch)      |
| OPTIONS | any                     | bare-bones CORS preflight (permissive for loopback use)  |

Every successful chat/text completion also returns a
`creation_os` object carrying the aggregator in use, the scalar σ,
σ_mean, σ_product, the 8-channel profile, and an `abstained` flag.
OpenAI SDK clients silently ignore unknown top-level keys so the
response remains a valid `chat.completion`; σ-aware clients (v108 and
the v107-installed curl examples) key on `creation_os` to surface the
gating signal to users.

## Config file

Path: `$HOME/.creation-os/config.toml`, overridable with `--config`.

```toml
[server]
host = "127.0.0.1"
port = 8080

[sigma]
tau_abstain = 0.7
aggregator  = "product"   # or "mean"

[model]
gguf_path = "~/.creation-os/models/bitnet-b1.58-2B-4T/ggml-model-i2_s.gguf"
n_ctx     = 2048
model_id  = "bitnet-b1.58-2B"   # optional; defaults to basename(gguf_path)

[web]
web_root = "web"
```

All keys are optional; unspecified keys fall back to the defaults
set in `cos_v106_config_defaults()`.  CLI flags beat the file; env
`COS_V101_AGG` beats both at the v101 aggregator layer.

The TOML loader is tiny (≈ 150 lines, zero deps).  It handles the
three sections above, strings with `"..."` or `'...'`, integers,
floats, and `#` comments.  Arrays and inline tables are silently
ignored; unknown keys are logged to stderr but not fatal.

## Streaming

`POST /v1/chat/completions` with `"stream": true` returns SSE framed
as:

```
data: {"id":"...","object":"chat.completion.chunk", ..., "choices":[{"delta":{"role":"assistant","content":"..."}, "finish_reason":null}]}

data: {"id":"...", ..., "choices":[{"delta":{}, "finish_reason":"stop"}], "creation_os":{...}}

data: [DONE]
```

For v106 we emit the full completion as one delta chunk, then a
terminator chunk carrying the σ annotation, then `[DONE]`.  True
token-by-token streaming requires a callback threaded into the v101
`_generate` path; that is a follow-up, not a v106 blocker.  Cursor
and Continue both accept the single-delta framing as a minimal valid
implementation of OpenAI's streaming protocol.

## σ-governance inside the response

```jsonc
{
  "id": "chatcmpl-1713…",
  "object": "chat.completion",
  "model": "bitnet-b1.58-2B",
  "choices": [...],
  "creation_os": {
    "sigma_aggregator": "product",
    "tau_abstain": 0.7,
    "sigma": 0.00412,
    "sigma_mean": 0.04218,
    "sigma_product": 0.00412,
    "sigma_profile": [0.87, 0.02, 0.01, 0.93, 0.44, 0.02, 0.01, 0.61],
    "abstained": false
  }
}
```

- `sigma` equals `sigma_product` unless the server was launched with
  `--aggregator mean` (or `[sigma] aggregator = "mean"` in the file).
- `abstained` is true iff `sigma > tau_abstain` at any token of the
  continuation; the response text is whatever the bridge had emitted
  up to that point, plus the stop marker.
- All eight channel values are per-continuation means (consistent
  with v103's sidecar semantics).

## Build + run

```bash
# Stub mode (no model, merge-gate safe):
make standalone-v106
./creation_os_server --self-test          # JSON helpers + config sanity
./creation_os_server --port 8080          # serves /health, /v1/models,
                                          # /v1/sigma-profile; /v1/chat
                                          # returns 503 until a GGUF is set

# Real mode (requires third_party/bitnet + models/):
make standalone-v106-real
./creation_os_server \
    --gguf models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf \
    --port 8080

# CI smoke (no model required):
make check-v106        # runs self-test + benchmarks/v106/check_v106.sh

# Real-model curl benchmark (requires GGUF):
make bench-v106        # benchmarks/v106/bench_v106.sh — five prompts,
                       # prints wall-time + σ_product per response
```

## Acceptance test for the directive

Directive PÄÄTÖS-PISTE v106: *"toimiiko curl-testi loopback:lla?"*

`make check-v106` is the pass/fail answer.  It exercises every
v106 surface except the inference path (which needs a GGUF):

- 9 `PASS` lines on a clean macOS / Linux tree with `creation_os_server`
  built.  That is what the directive asks for.
- `bench-v106` is the optional real-model check once the installer
  (v107) or a manual GGUF download has populated `models/`.

## What this does NOT ship

- Auth. `/v1/*` is open on the bind host.  Loopback default is safe;
  do not `--host 0.0.0.0` on an untrusted network without a reverse
  proxy doing auth.
- Multi-model routing.  `/v1/models` lists exactly one model (the
  one you launched the server with).  Multi-GGUF is v109's job.
- Function calling / tools.  OpenAI's tools surface is large; shipping
  it correctly is out of scope for v106.  The σ-governance is itself
  a form of tool (a refuse-action) and is surfaced through
  `creation_os.abstained`.
- True per-token streaming.  One-delta-then-terminator is a valid
  SSE implementation for the OpenAI contract; token-level streaming
  is a future hook on the v101 generate path.

## Open follow-ups

- Thread token-level streaming from `cos_v101_bridge_generate`
  through to SSE `delta.content` chunks.
- Add a `POST /v1/embeddings` endpoint when the bridge grows one.
- Expose the v102/v103 loglikelihood path as `/v1/score` for
  selective-prediction workflows that want raw ll + σ_profile per
  candidate continuation.

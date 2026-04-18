# v109 — Multi-GGUF (bridge is model-agnostic)

**Status:** v111 release · implemented as *serial* multi-GGUF; parallel
multi-tenant hosting is deliberately out of scope (see
[`src/v106/server.h`](../../src/v106/server.h)).

## What it promises

The v101 σ-bridge is a thin wrapper around `llama.cpp`.  Therefore:

- **Any GGUF `llama.cpp` can load, Creation OS can serve.**
- The `--gguf PATH` CLI flag of `creation_os_server` is model-agnostic;
  no model-specific code path exists in `src/v106/`.
- The eight σ-channels (v101) are computed from the logits/softmax of
  the loaded model — they are **architecture-invariant**.

## What it does not (yet) promise

- **Simultaneous multi-model hosting.**  A single `creation_os_server`
  process holds at most one `llama_context`.  `--gguf` accepts the
  syntax `--gguf A.gguf --gguf B.gguf` but only the last value is
  honored today; the earlier paths are logged and ignored.  Run N
  servers on N ports behind a proxy for multi-tenant.
- **Any specific benchmark number** on Llama / Qwen / Mistral.  The
  regression script only asserts the HTTP contract still holds; it
  does not attempt to reproduce upstream leaderboards.

## Regression — serial multi-GGUF

`benchmarks/v109/check_v109_multi_gguf.sh` probes every GGUF it can
find (either via `$COS_V109_GGUF_PATHS` or the standard
`~/.creation-os/models/` lookup).  For each it:

1. starts `creation_os_server --gguf $M` on a random free port
2. fetches `/v1/models`, asserts the OpenAI-shaped list
3. POSTs a 16-token chat completion, asserts `content` + `sigma*` fields
4. shuts down cleanly

The script is **merge-gate safe**: it SKIPs when no weights are on
disk, so a cold CI runner stays green.  To opt in locally:

```bash
export COS_V109_GGUF_PATHS="$HOME/models/Llama-3.1-8B.gguf:$HOME/models/Qwen2.5-7B.gguf:$HOME/models/Mistral-7B.gguf"
make check-v109-multi-gguf
```

## Reference invocations

```bash
# BitNet (default)
creation_os_server --gguf ~/.creation-os/models/bitnet-b1.58-2B-4T-Q4_K_M.gguf

# Llama-3.1
creation_os_server --gguf ~/models/Llama-3.1-8B-Instruct-Q4_K_M.gguf

# Qwen 2.5
creation_os_server --gguf ~/models/Qwen2.5-7B-Instruct-Q4_K_M.gguf

# Mistral
creation_os_server --gguf ~/models/Mistral-7B-Instruct-v0.3.Q4_K_M.gguf
```

## Claim discipline

What v109 justifies in the README and release notes:

> *"The bridge is model-agnostic: any GGUF `llama.cpp` supports,
>  Creation OS serves with the same eight-channel σ profile, the
>  same abstention τ, and the same `/v1/chat/completions` shape."*

What v109 does **not** justify:

- parallel serving of N models from one process (deferred)
- claims about model quality or downstream task scores (see
  [v111.1 frontier matrix](../v111/THE_FRONTIER_MATRIX.md) instead)

See [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md) for the
evidence-hygiene rules this doc follows.

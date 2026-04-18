# v112 — σ-Agent: σ-Gated Function Calling

OpenAI `tools` / `tool_choice` **API parity with a σ-governance layer**.

## The problem v112 solves

Every major agent framework today — OpenAI Swarm, LangGraph, CrewAI,
AutoGen — will call a tool whenever the model emits a plausible-looking
`tool_call` block, regardless of whether the model is *confident* about
which tool to call or *what arguments* to pass. When the routing is
wrong, it cascades: a bad tool choice produces a bad observation, the
next turn reasons from garbage, the trace drifts.

v112 adds a **measurement layer** between the model output and the tool
dispatcher:

1. User sends a standard OpenAI chat/completions request with a `tools`
   array.
2. The σ-bridge generates a response inside a `<tool_call>{…}</tool_call>`
   contract.
3. The v101 σ-channels (entropy, margin, n_effective, tail mass,
   max_token, mean, product, stability) are collected *over the
   generation window that produced the tool_call*.
4. σ_product is aggregated via v105; if σ_product > τ_tool (default
   `0.65`), the server **refuses to dispatch** the tool call and returns
   a diagnostic:

   ```json
   {
     "cos_abstained": true,
     "cos_abstain_reason": "low_confidence_tool_selection (channel=n_effective, σ=0.73>τ=0.65)",
     "cos_sigma_product": 0.73,
     "cos_collapsed_channel": "n_effective",
     "cos_parsed_tool": {"name": "execute_code", "arguments": "{…}"}
   }
   ```

5. The client — or the orchestrating agent — can react: tighten the
   prompt, relax `tau_tool`, or grant explicit consent.

## What it is **not**

- Not a replacement for any existing tool library; the `tools` schema
  is OpenAI-shape and you can drop v112 in front of a LangChain or
  LlamaIndex agent without changing their tool adapters.
- Not a learned classifier. The σ-channels are the same ones v101
  already measures; v112 just surfaces them at a decision boundary
  that every other framework ignores.

## API surface

```
POST /v1/chat/completions
{
  "model":       "any-loaded-gguf",
  "messages":    [{"role": "user", "content": "..."}],
  "tools":       [{"type": "function", "function": {...}}, ...],
  "tool_choice": "auto" | "none" | {"type":"function","function":{"name":"..."}},
  "tau_tool":    0.65   // optional; default 0.65, clamped to [0,1]
}
```

When `tools` is present, the response is the OpenAI standard
`choices[0].message.tool_calls[…]` shape *plus* a `creation_os.tool_gate`
block with the σ telemetry:

```json
"creation_os": {
  "tool_gate":     true,
  "tau_tool":      0.65,
  "sigma_product": 0.22,
  "sigma_mean":    0.20,
  "abstained":     false
}
```

## Source

- [`src/v112/tools.h`](../../src/v112/tools.h) — API
- [`src/v112/tools.c`](../../src/v112/tools.c) — parser + σ-gate + JSON
- [`src/v112/main.c`](../../src/v112/main.c) — CLI wrapper
- [`src/v106/server.c`](../../src/v106/server.c) — server integration
  (see `maybe_handle_tools`)
- [`benchmarks/v112/scenarios.jsonl`](../../benchmarks/v112/scenarios.jsonl)
  — 10 function-calling scenarios (weather, math, DB lookup, file read,
  API call, code execute, email, multi-tool, forced choice, no-tool
  prose)

## Self-test

```bash
make check-v112
```

runs:

1. `creation_os_v112_tools --self-test` — 29 pure-C assertions covering
   tools/tool_choice parsing, `<tool_call>` text extraction, σ-gate
   thresholding, and OpenAI-shaped JSON emission.
2. `creation_os_v112_tools --scenarios benchmarks/v112/scenarios.jsonl`
   — exercises the full pipeline on every scenario in stub mode
   (no GGUF needed; σ-gate abstains with `no_model_loaded`, which is
   the correct behaviour).

## Claim discipline

- The σ-gate behaviour is **defined** by `τ_tool` and the v101 σ-channels.
  We do not claim to outperform OpenAI Swarm on any measured benchmark
  in v112.0 — the claim is that v112 **exposes a signal** (σ-product on
  the tool_call window) that Swarm, LangGraph, CrewAI, and AutoGen do
  not expose. Exposure ≠ correctness; verifying that σ-gated routing
  prevents cascades on a standardised agent benchmark (e.g.
  AgentBench, τ-bench, WebArena) is scheduled for v116.
- We do **not** route blindly on role match; the stub runner used by CI
  demonstrates the arbitration contract, not a real routing policy.

## σ-stack layer

v112 sits at the **σ-governance** layer of the Creation OS six-layer
stack: it consumes the v101 σ-channels and the v105 aggregator, and it
feeds the v108 web UI (tool-call abstention is rendered as a red
"σ-gate" pill).

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_

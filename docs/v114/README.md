# v114 — σ-Swarm: Multi-Specialist σ-Routed Orchestration

The **Proconductor principle shipped as a product**: several small,
specialist GGUF models each answer the user's task; the one with the
lowest σ_product wins, and — when two specialists independently agree
at low σ — we flag **resonance** (not "consensus": consensus is a vote,
resonance is a measurement).

## Why v114 exists

The Nature 2026 review of multi-agent LLM systems identified *"small
routing mistakes cascade into systemic misbehaviour"* as the core
failure mode of multi-agent pipelines (OpenAI Swarm, LangGraph, CrewAI,
AutoGen). The proposed mitigations always assume you can rank agents
**after the fact** with a critic model. That adds latency, a second
unreliable model, and does not fix the original routing mistake.

v114 takes a different route: if every specialist already exposes a
calibrated σ-product, the router can **pick the most-confident answer**
without a critic. When two specialists agree *and* both are confident,
that's stronger evidence than either alone ("resonance"). When all of
them are uncertain, the swarm abstains with an honest signal instead of
hallucinating a vote.

## Architecture

- **Orchestrator**: the v106 σ-Server.
- **Specialists**: v109 multi-GGUF loader, one `cos_v101_bridge_t` per
  configured model.
- **Router**: `cos_v114_arbitrate()` — σ_product-min with a resonance
  epsilon.
- **Exposure**: every swarm response carries these HTTP headers,

  ```
  X-COS-Specialist:        bitnet-2b-reasoning
  X-COS-Specialist-Sigma:  0.23
  X-COS-Runner-Up:         qwen-7b-code
  X-COS-Runner-Up-Sigma:   0.41
  X-COS-Consensus:         true | false
  X-COS-Parallel-Mode:     sequential | parallel
  X-COS-Routing:           sigma_product_min
  ```

  plus a `creation_os.swarm.candidates[]` block inside the JSON body
  listing every specialist's σ so clients can visualise the race.

## Config

`~/.creation-os/config.toml`:

```toml
[swarm]
specialists = [
  { name = "reasoning", gguf = "bitnet-2b.gguf",  role = "math, logic" },
  { name = "code",      gguf = "qwen-7b.gguf",    role = "programming" },
  { name = "general",   gguf = "llama-8b.gguf",   role = "knowledge, chat" }
]
routing             = "sigma_product_min"
consensus_threshold = 2
consensus_epsilon   = 0.15
```

`consensus_threshold = N` means ≥ N specialists must emit byte-equal
content within an epsilon of σ-product to flag resonance.

## v114.0 honesty

Current v114.0 scope:

- ✅ TOML `[swarm]` parser.
- ✅ Arbitrator (`cos_v114_arbitrate`).
- ✅ Deterministic **stub runner** that produces per-specialist σ from
  `(prompt, role, name)` hashes so CI can exercise the whole stack
  with zero GGUF weights.
- ✅ Response builder (headers + JSON).
- ⚠️ **Sequential**, not parallel, inference. `llama_context` is not
  thread-safe; a true parallel swarm requires one process per
  specialist. v114.1 will ship this as `cos_v114_spawn_workers()` that
  fanouts over Unix-domain sockets. Until then, `X-COS-Parallel-Mode:
  sequential` is set in every response so consumers know.
- ⚠️ Resonance detection today is **string-equality** on the winner's
  content; a semantic resonance detector (embedding cosine ≥ τ) is
  planned for v115.

We document the limitations on the tin so the σ-swarm claim is honest:
**the measurement infrastructure and the API are live; the parallel
execution optimisation is deferred.**

## Source

- [`src/v114/swarm.h`](../../src/v114/swarm.h) — API
- [`src/v114/swarm.c`](../../src/v114/swarm.c) — parser + arbitrator +
  stub runner + response builder
- [`src/v114/main.c`](../../src/v114/main.c) — CLI wrapper
- [`benchmarks/v114/swarm.toml`](../../benchmarks/v114/swarm.toml) —
  example config
- [`benchmarks/v114/check_v114_swarm_routing.sh`](../../benchmarks/v114/check_v114_swarm_routing.sh)
  — routing smoke (code prompt → code specialist, math prompt →
  reasoning specialist, header contract)
- [`benchmarks/v114/check_v114_consensus.sh`](../../benchmarks/v114/check_v114_consensus.sh)
  — consensus-header + JSON-body agreement

## Self-test

```bash
make check-v114
```

runs `check-v114-swarm-routing` and `check-v114-multi-specialist-consensus`
in sequence.

## Claim discipline

- We do **not** claim "4×500M > 1×70B" as a measured result in v114.0.
  The software routes among whatever specialists you configure; a
  measured comparison requires a standardised benchmark with GGUF
  weights for each tier and an established harness. It is scheduled
  for v117 in [docs/RESEARCH_AND_THESIS_ARCHITECTURE.md](../../docs/RESEARCH_AND_THESIS_ARCHITECTURE.md).
- We **do** claim: the σ-routed arbitrator is deterministic, exposes
  every specialist's σ to the client, and abstains honestly when
  all specialists are uncertain. That's a software contract, not a
  benchmark claim.

## σ-stack layer

v114 sits at the **Reasoning** layer (multi-path selection, as in
v111.2 /v1/reason) and the **σ-governance** layer (σ-product as the
routing key).

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_

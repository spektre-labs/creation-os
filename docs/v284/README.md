# v284 — σ-Multi-Agent

**Framework-agnostic σ-layer** for agent orchestration.

LangGraph, CrewAI, AutoGen, OpenAI Swarm — six frameworks compete
on routing and none of them measure σ.  Creation OS sits as
middleware **above** every one of them: an agent emits a decision
event, the σ-gate evaluates it, and the action is allowed, deferred,
or escalated.

## σ innovation (what σ adds here)

> **σ is the one signal that travels with every agent-to-agent
> hand-off AND decides how many agents to spend on a task AND
> weights consensus by reliability** — framework-agnostic, failure
> bounded, cost-optimal.

## v0 manifests

Enumerated in [`src/v284/multi_agent.h`](../../src/v284/multi_agent.h);
pinned by [`benchmarks/v284/check_v284_multi_agent_sigma_layer.sh`](../../benchmarks/v284/check_v284_multi_agent_sigma_layer.sh).

### 1. Framework adapters (exactly 4 canonical rows)

`langgraph · crewai · autogen · swarm`, each
`adapter_enabled == true` AND `sigma_middleware == true`.

Contract: canonical order; 4 distinct; every adapter live.

### 2. Agent-to-agent σ (exactly 4 fixtures, τ_a2a = 0.40)

Each: `sender_id`, `receiver_id`, `σ_message ∈ [0, 1]`,
`decision ∈ {TRUST, VERIFY}`, rule
`σ_message ≤ τ_a2a → TRUST else VERIFY`.

Contract: ≥ 1 TRUST AND ≥ 1 VERIFY — low-σ messages are trusted,
high-σ messages are re-verified, stopping error cascades in the
mesh.

### 3. σ-weighted consensus (exactly 5 agents)

Each: `agent_id`, `σ_agent ∈ [0, 1]`, `weight`, `is_winner`.

`weight_i = (1 − σ_agent_i) / Σ_j (1 − σ_agent_j)` → Σ weights = 1.0
(± 1e-3).  `is_winner == true` iff `agent_id == argmin_i σ_agent_i`
(equivalently `argmax_i weight_i`).

Contract: Σ weights = 1.0; argmin σ and argmax weight select the
same agent; exactly one `is_winner`.

### 4. Cost-optimal routing (exactly 4 canonical tiers)

| tier | σ cascade | mode | n_agents |
|------|:---:|:---:|:---:|
| `easy`      | ≤ 0.20 | `LOCAL`      | 1 |
| `medium`    | ≤ 0.50 | `NEGOTIATE`  | 2 |
| `hard`      | ≤ 0.80 | `CONSENSUS`  | 5 |
| `critical`  | else   | `HITL`       | 0 |

Contract: each tier fires exactly once (permutation of the four
modes); n_agents matches mode.

### σ_multiagent (surface hygiene)

```
σ_multiagent = 1 −
  (adapter_rows_ok + adapter_distinct_ok +
   a2a_rows_ok + a2a_both_ok +
   consensus_rows_ok + consensus_weights_ok +
   consensus_argmax_ok + routing_rows_ok +
   routing_distinct_ok) /
  (4 + 1 + 4 + 1 + 5 + 1 + 1 + 4 + 1)
```

v0 requires `σ_multiagent == 0.0`.

## Merge-gate contracts

- 4 adapter rows canonical; all enabled AND σ-middleware on every
  row; all distinct.
- 4 a2a rows; decision follows σ vs τ_a2a; both branches fire.
- 5 consensus rows; weights sum to 1.0 ± 1e-3; winner == argmin σ
  == argmax weight; exactly 1 winner.
- 4 routing rows canonical; each mode fires exactly once; n_agents
  matches mode.
- `σ_multiagent ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** types adapter / a2a / consensus / routing as
  predicates on enumerated fixtures — no live framework wiring, no
  real agent traffic, no measured HITL latency.
- **v284.1 (named, not implemented)** is live middleware adapters
  for LangGraph / CrewAI / AutoGen / Swarm with a typed hand-off
  event, measured `σ_message` on real agent traffic, and a
  production HITL escalation path on the `critical` tier.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

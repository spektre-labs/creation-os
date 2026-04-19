# v282 — σ-Agent (`docs/v282/`)

Autonomous agent with σ on every action, chain, tool,
and failure.  Agents (tool use, multi-step planning,
long-horizon tasks) are the 2026 headline; σ is what
makes them trustworthy.  v282 types the σ-layer as
four merge-gate manifests.

v282 does not run an agent.  It types per-action
AUTO / ASK / BLOCK, multi-step σ-confidence
propagation, tool-selection USE / SWAP / BLOCK, and
failure recovery with a strictly increasing σ and an
applied σ-gate update.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Action gate (exactly 4 fixtures, τ_auto = 0.20, τ_ask = 0.60)

Three-way cascade:
`σ_action ≤ τ_auto → AUTO` else
`σ_action ≤ τ_ask  → ASK`  else
`BLOCK`.  Every branch fires ≥ 1×.

| action_id | σ_action | decision |
|----------:|---------:|:--------:|
| 0         | 0.08     | AUTO     |
| 1         | 0.35     | ASK      |
| 2         | 0.55     | ASK      |
| 3         | 0.82     | BLOCK    |

### Propagation (exactly 2 chains, τ_chain = 0.70)

`σ_total = 1 − (1 − σ_per_step)^n_steps`; verdict
`PROCEED iff σ_total ≤ τ_chain` else `ABORT`.  The
chain math is a merge-gate predicate (match within
`1e-4`) so a regression to a wrong formula fails
byte-exactly.  PROCEED and ABORT each fire exactly
once.

| label   | n_steps | σ_per_step | σ_total  | verdict  |
|---------|--------:|-----------:|---------:|:--------:|
| `short` | 3       | 0.10       | ~0.271   | PROCEED  |
| `long`  | 10      | 0.30       | ~0.972   | ABORT    |

### Tool selection (exactly 3 fixtures, canonical order, τ_tool_use = 0.30, τ_tool_swap = 0.60)

Three-way cascade on σ_tool:
`σ_tool ≤ τ_tool_use  → USE`  else
`σ_tool ≤ τ_tool_swap → SWAP` else
`BLOCK`.  Every branch fires ≥ 1×.

| tool_id        | σ_tool | decision |
|----------------|-------:|:--------:|
| `correct`      | 0.10   | USE      |
| `wrong_light`  | 0.45   | SWAP     |
| `wrong_heavy`  | 0.78   | BLOCK    |

### Recovery (exactly 3 fixtures)

σ strictly increases after a failure on the same
context AND the σ-gate update is applied for every
row — the agent has to learn from the failure.

| context_id | σ_first_try | σ_after_fail | gate_update_applied |
|-----------:|------------:|-------------:|:-------------------:|
| 0          | 0.15        | 0.45         | true                |
| 1          | 0.20        | 0.60         | true                |
| 2          | 0.30        | 0.72         | true                |

### σ_agent

```
σ_agent = 1 − (action_rows_ok + action_all_branches_ok +
               prop_rows_ok + prop_math_ok +
               prop_both_ok +
               tool_rows_ok + tool_all_branches_ok +
               recovery_rows_ok +
               recovery_monotone_ok +
               recovery_updates_ok) /
              (4 + 1 + 2 + 1 + 1 + 3 + 1 + 3 + 1 + 1)
```

v0 requires `σ_agent == 0.0`.

## Merge-gate contract

`bash benchmarks/v282/check_v282_agent_action_sigma_gate.sh`

- self-test PASSES
- 4 action rows; cascade AUTO / ASK / BLOCK fires
  all three branches
- 2 chain rows canonical (short, long); math matches
  product formula within `1e-4`; PROCEED and ABORT
  each fire exactly once
- 3 tool rows canonical (correct, wrong_light,
  wrong_heavy); cascade USE / SWAP / BLOCK fires all
  three branches
- 3 recovery rows; σ_after_fail > σ_first_try
  strictly per row; gate_update_applied on every row
- `σ_agent ∈ [0, 1]` AND `σ_agent == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed action / chain / tool /
  recovery manifest with FNV-1a chain.
- **v282.1 (named, not implemented)** — live agent
  runtime wired into v262 with σ-gated MCP tool
  calls, real multi-step plan propagation,
  production fail / re-evaluate loop hooked into
  v275 TTT weight updates, and a measured abstention
  curve (AURCC) on a human-preference benchmark.

## Honest claims

- **Is:** a typed, falsifiable σ-agent manifest where
  per-action gating, multi-step σ propagation,
  tool-selection cascade, and σ-monotone recovery
  are merge-gate predicates.
- **Is not:** a running agent, nor a measurement of
  abstention vs human preference, tool-call accuracy,
  or production failure rate.  v282.1 is where the
  manifest meets real MCP invocations.

---

*Spektre Labs · Creation OS · 2026*

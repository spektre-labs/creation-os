# v121 — σ-Planning: HTN + MCTS with σ-Aware Backtracking

v111.2 `/v1/reason` is a single-turn reasoning loop. v121 lifts that
loop into a **multi-step planner** that decomposes a goal into
subtasks, routes each subtask to a tool (v112 function calling, v113
sandbox, v114 swarm, v115 memory), measures σ at every transition,
and **backtracks MCTS-style** on any step whose σ exceeds τ.

## The problem v121 solves

LangGraph and CrewAI are deterministic graph / role executors:
they have no principled way to *change their mind* when a step
misbehaves. OpenAI Swarm routes blindly. HTN planners from classical
AI (NOAH, SHOP2) have no notion of model uncertainty.

v121 combines the HTN decomposition structure with an MCTS-style
backtrack policy keyed on three σ-channels:

- **σ_decompose**: was this subtask the right slice of the goal?
- **σ_tool**:      was the tool choice right for this subtask?
- **σ_result**:    does the executor's output satisfy the subtask?

Aggregation is the geometric mean — identical to v6/v7's
σ_product convention — so `τ_step = 0.60` compares on the same
scale as per-channel thresholds.

## The planner contract

```
for each step:
    candidates ← source(goal, step_index)     # HTN decomposition
    pick branch with lowest predicted σ_pre   # MCTS selection
    execute, observe σ_step
    if σ_step ≤ τ_step:
        commit, advance
    else:
        backtrack, pick next-lowest branch
        (count a replan)
if replans exhausted → abort the whole plan with diagnostics
```

Replans are bounded (default 8) so no plan diverges.

## Response shape (`/v1/plan`)

```json
{
  "goal": "Analyze this CSV and create a summary report",
  "plan": [
    { "step": 0, "action": "read_file_via_memory", "tool": "memory",
      "branch": 2, "branches": 3,
      "sigma_decompose": 0.55, "sigma_tool": 0.55,
      "sigma_result":    0.30, "sigma_step": 0.45,
      "replanned": true },
    { "step": 1, "action": "analyze_data",  "tool": "sandbox", ... },
    { "step": 2, "action": "generate_report","tool": "chat",   ... }
  ],
  "total_sigma": 0.39,
  "max_sigma":   0.45,
  "replans":     2,
  "aborted":     false
}
```

## What ships in v121.0

- Pure-C HTN/MCTS planner with a deterministic step-source callback
  contract, supporting up to 32 steps and 4 branches per step.
- σ-aware branch selection (lowest predicted `σ_pre = √(σ_dec·σ_tool)`)
  plus post-execution σ-gate enforcement.
- Synthetic happy + pathological test sources in the self-test:
  three-subtask plan that forces two replans at step 0, and a
  pathological source whose branches all fail σ and correctly drives
  the planner into `aborted=true`.
- `/v1/plan`-shaped JSON writer plus a CLI (`--plan "GOAL"`) that
  emits the synthetic plan for smoke testing.

## What lands in v121.1

- Replace the stub step-source with a real decomposer: BitNet 2B via
  v101-bridge proposes 3 candidate decompositions per step, tool
  routing uses v114's σ_product router, `σ_result_on_execute` comes
  from the executor's own σ (v106's aggregator).
- HTTP surface: `POST /v1/plan` on v106 returning the exact JSON
  above, plus a live Web UI (v108) view of the plan tree as it
  unfolds.
- Persistence: commit each plan + σ trail to v115 episodic memory so
  the swarm can learn from its own backtracks.

## Source

- [`src/v121/planning.h`](../../src/v121/planning.h)
- [`src/v121/planning.c`](../../src/v121/planning.c) — lowest-σ
  selection, σ-step gate, backtrack bookkeeping, JSON writer
- [`src/v121/main.c`](../../src/v121/main.c) — CLI with a
  deterministic step-source mirroring the self-test
- [`benchmarks/v121/check_v121_planning_loop.sh`](../../benchmarks/v121/check_v121_planning_loop.sh)

## Self-test

```bash
make check-v121
```

Runs the pure-C self-test (7 assertions across the happy + abort
paths) plus a smoke that verifies the `/v1/plan` JSON contract,
asserts `replans ≥ 2`, and enforces `max_sigma ≤ τ_step` on every
committed step.

## Claim discipline

- v121.0 proves the **control flow**: decomposition, branch
  selection, MCTS backtrack on σ, bounded replans, abort path, JSON
  contract.
- v121.0 does **not** claim real task completion rates or comparisons
  against CrewAI / LangGraph. Those numbers require the v121.1
  wire-up to live BitNet + real tools; they go through the standard
  REPRO bundle with host metadata.
- The σ-channels are honest model-uncertainty proxies at the
  planner layer; they are not a substitute for tool-level guards
  (v113 sandbox seccomp, v112 function-schema validation).

## σ-stack layer

v121 sits at **Reasoning + Agentic**, above v111.2 (single-turn
reasoning) and v112/v113/v114 (tool / sandbox / swarm). Its σ
feeds the same v105 aggregator as everything else.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_

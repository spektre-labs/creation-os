# v207 σ-Design — space exploration + constraints + Pareto + receipt

## Problem

Science produces knowledge; engineering turns it into a
design. v207 is the σ-governed design space: candidates
are checked against hard constraints from v199 law, v191
constitutional, v200 market; the feasible subset is
reduced to a multi-objective Pareto front; every decision
is receipted.

## σ-innovation

12 design candidates over three axes:

| axis        | direction |
|-------------|-----------|
| performance | maximise  |
| complexity  | minimise  |
| cost        | minimise  |

Hard constraints (booleans per candidate):

| flag        | source |
|-------------|--------|
| `law_ok`    | v199 norm register |
| `ethics_ok` | v191 constitutional AI |
| `cost_ok`   | cost ≤ `C_max` (v200 market budget) |

A candidate is **feasible** iff all three hold; otherwise
**INFEASIBLE**. σ_feasibility blends cost headroom with
fixed penalties per hard violation so the honesty signal
degrades smoothly rather than snapping.

Pareto membership: a feasible candidate `p` is on the
front iff no other feasible candidate strictly dominates
it on (performance↑, complexity↓, cost↓).

Receipt: FNV-1a chain covers
`(id, law, ethics, cost_ok, feasible, pareto, perf, cplx,
cost, σ_feasibility, prev)`. This is the
`requirements → design → rationale → implementation →
test` spine feeding v181.

## Merge-gate

`make check-v207` runs
`benchmarks/v207/check_v207_design_pareto_front.sh` and
verifies:

- self-test PASSES
- 12 candidates, ≥ 3 on the Pareto front
- ≥ 1 INFEASIBLE (in the fixture: one each for law,
  ethics, cost)
- `pareto ⇒ feasible`; no feasible candidate dominates
  any Pareto member (verified in Python as well as C)
- σ_feasibility ∈ [0, 1]
- chain valid + byte-deterministic

## v207.0 (shipped) vs v207.1 (named)

|                     | v207.0 (shipped) | v207.1 (named) |
|---------------------|------------------|----------------|
| search              | 12-candidate fixture | v163 CMA-ES |
| generation          | synthetic attributes | v151 code-agent implementation |
| test                | — | v113 sandbox |
| reflection          | — | v147 reflect |
| constraints         | static booleans | live v199/v191/v200 feeds |

## Honest claims

- **Is:** a deterministic Pareto design explorer with
  hard legal / ethical / cost gating and a receipted
  decision trail.
- **Is not:** a CMA-ES optimiser, a live code-gen
  pipeline, or a sandbox runner — those ship in v207.1.

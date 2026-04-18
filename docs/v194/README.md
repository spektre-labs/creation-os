# v194 σ-Horizon — multi-horizon goal tree + degradation monitor

## Problem

Long-horizon agents in the field collapse after ~35 minutes
because they never measure their own degradation. σ measures
it, but only if the system is built around σ from the start.

## σ-innovation

Three-tier goal tree with per-tier σ-estimates:

| tier | horizon | example |
|------|---------|---------|
| strategic   | week+ | “Build Creation OS v200” |
| tactical    | day   | “Implement v194–v198” |
| operational | now   | “Write `src/v194/horizon.c`” |

On the operational tier a σ-slope monitor runs over a sliding
`window=10` window; when the window is **strictly monotone**
and its mean slope exceeds `τ_degrade_slope`, a recovery
ladder fires in order:

| step | operator | σ discount |
|------|----------|------------|
| 1 | v117 KV-cache flush     | −0.10 |
| 2 | v172 summarize + resume | −0.20 |
| 3 | v115 break-point + save | −0.35 |

Every operational task emits a FNV-1a-chained **checkpoint**.
A simulated crash-recovery pass (rebuilding the chain from
scratch) reproduces the terminal hash byte-identically —
work is never lost.

## Merge-gate

`make check-v194` runs
`benchmarks/v194/check_v194_horizon_degradation_detect.sh`
and verifies:

- self-test PASSES
- tree shape exactly 1 / 3 / 12 (strategic / tactical / operational)
- `degradation_detected == true`
- `recovery_ladder == [1, 2, 3]` (ordered)
- σ after the full ladder strictly lower than σ at detection
- exactly one tick carries `degradation = true`
- checkpoint chain valid + crash-recovery reproduces terminal hash
- byte-deterministic

## v194.0 (shipped) vs v194.1 (named)

| | v194.0 | v194.1 |
|-|-|-|
| goal tree | in-memory fixture | live v115 memory persistence |
| `cos goals` CLI | — | walker over `~/.creation-os/goals.jsonl` |
| ladder operators | closed-form σ discounts | real v117/v172/v115 invocations |
| audit | FNV-1a chain in RAM | streamed into v181 |

## Honest claims

- **Is:** a deterministic kernel that builds a 1/3/12 goal tree,
  runs a 30-tick operational trajectory, fires the σ-slope
  degradation monitor, walks the recovery ladder in order,
  and proves crash-recovery reproduces the terminal checkpoint
  hash.
- **Is not:** a durable goal store or an online v117/v172/v115
  integration — those ship in v194.1.

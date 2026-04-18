# v200 σ-Market — σ as the price signal

## Problem

Cost accounting for a reasoning stack is usually
per-call, per-token, or per-GPU-hour — none of which
measures whether the call was needed. v200 makes σ itself
the price signal: high σ (high uncertainty) is expensive
because the system actually needs the large model; low σ
is cheap because the local path suffices.

## σ-innovation

Four resources tracked on a deterministic ledger:

| resource | unit | pool size |
|----------|------|-----------|
| `R_COMPUTE`  | cpu-seconds (v189 TTC budget)       | 1000 |
| `R_API`      | external model calls                 | 200 |
| `R_MEMORY`   | v115 SQLite bytes (tight by design)  | 50 |
| `R_ADAPTERS` | v145 LoRA slots                      | 20 |

Price law:

```
price   = σ_before
penalty = hold_fraction   if hold_fraction > τ_hoard (0.20)
        = 0               otherwise
cost    = price · (1 + penalty)
```

Route choice (σ_local = 0.35):
- `σ > σ_local`  → route = API (larger/external model)
- `σ ≤ σ_local` → route = local

When a single allocator exceeds τ_hoard of any pool, v200
triggers a deterministic eviction that collapses the
hoarder's holdings to `0.5·τ_hoard·pool` — **anti-hoarding
is a property of the ledger itself**, not a policy.

Self-improving cost: across the 40-query trajectory
`σ_before` falls monotonically (0.78 → 0.04), so
`cost_second_half` is strictly cheaper than
`cost_first_half`. Every API call drops σ by 0.15
(v120-distill signal); every local call shaves 0.05.

## Merge-gate

`make check-v200` runs
`benchmarks/v200/check_v200_market_scarcity_penalty.sh` and
verifies:

- self-test PASSES
- 4 resources × 40 queries
- σ > σ_local ⇒ route = API, else local
- `cost_second_half < cost_first_half` (self-improving)
- ≥ 1 scarcity-penalty tick + ≥ 1 eviction
- exchange log chain valid + byte-deterministic

## v200.0 (shipped) vs v200.1 (named)

| | v200.0 | v200.1 |
|-|--------|--------|
| compute | synthetic | wired to v189 TTC budget |
| API metrics | fixture | live v120 distill latency |
| memory | fixed pool | live v115 SQLite sizing |
| adapters | fixed pool | v145 LoRA capacity probe |
| audit | FNV-1a chain | streamed to v181 audit |

## Honest claims

- **Is:** a deterministic 40-query demonstrator where σ
  deterministically sets price, the ledger triggers
  scarcity penalties and evictions on schedule, and
  self-improving cost is measured over the trajectory.
- **Is not:** a live cost accountant or production
  billing ledger — those ship in v200.1.

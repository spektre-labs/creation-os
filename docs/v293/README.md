# v293 — σ-Hagia-Sofia

**Continuous use = longevity.**

Hagia Sofia has stood for 1500 years because someone was always
using it: cathedral → mosque → museum → mosque.  Code lives the
same way: a kernel in continuous use survives vendor changes,
license pivots, and ecosystem fashion.  v293 types longevity
discipline as four v0 manifests: adoption is measured, kernels
serve multiple domains, community owns the long tail, and
re-purposing keeps the stones in service.

## σ innovation (what σ adds here)

> **σ-gate is domain-agnostic.**  One σ-gate serves LLMs, sensors,
> and organizations — a single function in three rooms.  v293 types
> the adoption panel that keeps the building open.

## v0 manifests

Enumerated in [`src/v293/hagia_sofia.h`](../../src/v293/hagia_sofia.h);
pinned by
[`benchmarks/v293/check_v293_hagia_sofia_continuous_use.sh`](../../benchmarks/v293/check_v293_hagia_sofia_continuous_use.sh).

### 1. Adoption metrics (exactly 3 canonical metrics, all tracked)

`daily_users · api_calls · sigma_evaluations`, each
`tracked == true` — v293 names the instrument panel for
`cos status --adoption`.  Matches merge-gate condition "Adoption
tracking aktiivinen".

### 2. Multi-purpose kernels (exactly 3 canonical domains)

`llm · sensor · organization`, each
`sigma_gate_applicable == true` — one function, many rooms.

### 3. Community longevity (exactly 3 canonical properties)

`open_source_agpl · community_maintainable · vendor_independent`,
each `holds == true`.

### 4. Adaptive re-purposing lifecycle (exactly 3 canonical phases, all alive)

| phase                       | use_count | alive  | warning_issued | new_domain_found |
|-----------------------------|-----------|--------|----------------|------------------|
| `active_original_purpose`   | 100       | `true` | `false`        | `false`          |
| `declining_usage`           | 20        | `true` | `true`         | `false`          |
| `repurposed`                | 80        | `true` | `false`        | `true`           |

Contract: the `declining_usage` row carries a warning; the
`repurposed` row finds a new domain — the lifecycle has a
documented re-use, not a retirement.

### σ_hagia (surface hygiene)

```
σ_hagia = 1 −
  (metric_rows_ok + metric_all_tracked_ok +
   domain_rows_ok + domain_all_applicable_ok +
   community_rows_ok + community_all_hold_ok +
   lifecycle_rows_ok + lifecycle_all_alive_ok +
   lifecycle_warning_ok + lifecycle_new_domain_ok) /
  (3 + 1 + 3 + 1 + 3 + 1 + 3 + 1 + 1 + 1)
```

v0 requires `σ_hagia == 0.0`.

## Merge-gate contracts

- 3 adoption metrics canonical; all tracked.
- 3 multi-purpose domains canonical; σ-gate applicable on every
  row.
- 3 community properties canonical; all hold.
- 3 lifecycle phases canonical; all alive; declining warns;
  repurposed finds new domain.
- `σ_hagia ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the adoption / multi-purpose /
  community / lifecycle contracts as predicates.  No live
  telemetry, no repurposing registry.
- **v293.1 (named, not implemented)** is live adoption telemetry
  driving `cos status --adoption`, automatic warnings when a
  kernel's `use_count` falls below threshold, and a re-purposing
  registry that tracks how a single kernel evolves across domains
  over time.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

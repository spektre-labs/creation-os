# v262 — σ-Hybrid-Engine (`docs/v262/`)

Multi-engine σ-routing.  Five engines — BitNet (fast /
local), AirLLM (heavy / local), Engram (O(1) facts /
local), api-claude, api-gpt — plugged into one σ-driven
router with a cascade and a cost report.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Engine registry (exactly 5, canonical order)

| engine              | tier          | €/1 k | σ_floor |
|---------------------|---------------|------:|--------:|
| `bitnet-3B-local`   | `local_fast`  | 0.000 | 0.40    |
| `airllm-70B-local`  | `local_heavy` | 0.000 | 0.30    |
| `engram-lookup`     | `local_fact`  | 0.000 | 0.20    |
| `api-claude`        | `cloud`       | 0.015 | 0.15    |
| `api-gpt`           | `cloud`       | 0.010 | 0.18    |

### σ-routing fixtures (4, ≥ 3 distinct engines)

| request                 | σ_difficulty | chosen                |
|-------------------------|-------------:|-----------------------|
| `small_helper_query`    | 0.12         | `bitnet-3B-local`     |
| `capital_of_finland`    | 0.05         | `engram-lookup`       |
| `explain_relativity`    | 0.45         | `airllm-70B-local`    |
| `write_legal_brief`     | 0.78         | `api-claude`          |

### Cascade (exactly 4 steps, τ_accept = 0.40)

Rule: `σ_result ≤ τ_accept` → `OK`, else → `ESCALATE`.

| step | engine              | σ_result | decision   |
|-----:|---------------------|---------:|------------|
| 0    | `bitnet-3B-local`   | 0.60     | `ESCALATE` |
| 1    | `airllm-70B-local`  | 0.30     | `OK`       |
| 2    | `bitnet-3B-local`   | 0.10     | `OK`       |
| 3    | `api-claude`        | 0.08     | `OK`       |

Contract: step 0 MUST be `ESCALATE` (cascade has teeth),
≥ 1 `OK`, ≥ 1 cloud-tier step (fallback works).

### Cost report (monthly)

| field               | value  |
|---------------------|-------:|
| `local_pct`         | 87     |
| `api_pct`           | 13     |
| `eur_sigma_route`   | 4.20   |
| `eur_api_only`      | 32.00  |
| `savings_pct`       | 87     |

`savings_pct ≈ 100 × (eur_api_only − eur_sigma_route) /
eur_api_only`.  Contract: `local_pct + api_pct == 100`,
`local_pct ≥ 80`, `savings_pct ≥ 80`.

### σ_hybrid_engine

```
σ_hybrid_engine = 1 − (engines_ok + routes_ok +
                       distinct_engines_ok + cascade_ok +
                       cost_ok) /
                      (5 + 4 + 1 + 4 + 1)
```

v0 requires `σ_hybrid_engine == 0.0`.

## Merge-gate contract

`bash benchmarks/v262/check_v262_hybrid_engine_routing.sh`

- self-test PASSES
- 5 engines in canonical order; every `engine_ok`
- 4 routing fixtures with `chosen_engine` ∈ registry;
  ≥ 3 distinct engines used
- 4 cascade steps; decision matches σ vs τ_accept;
  step 0 MUST be `ESCALATE`; ≥ 1 `OK` AND ≥ 1 cloud step
- cost report: `local_pct + api_pct == 100`,
  `local_pct ≥ 80`, `savings_pct ≥ 80`, rounding within
  ±1 pt
- `σ_hybrid_engine ∈ [0, 1]` AND `σ_hybrid_engine == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed engine / route / cascade /
  cost manifest with FNV-1a chain.
- **v262.1 (named, not implemented)** — live `cos
  engines` CLI with backend auto-detection, real-time
  σ-router wired to v243 pipeline, actual invoice
  reconciliation for api-claude / api-gpt, σ-proxy cost
  optimiser as a cascade driver.

## Honest claims

- **Is:** a typed, falsifiable routing manifest where
  the cascade, the cost ratio, and the per-route
  distinct-engine count are merge-gate predicates.
- **Is not:** a live multi-engine runtime.  v262.1 is
  where the manifest drives real traffic.

---

*Spektre Labs · Creation OS · 2026*

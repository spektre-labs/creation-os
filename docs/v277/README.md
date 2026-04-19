# v277 — σ-Distill-Runtime (`docs/v277/`)

Live teacher → student distillation with a σ-filter on
what is worth learning, a σ-routed domain manifest, and
a trajectory that shows API share falling while the
local BitNet student absorbs the workload.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Teacher / student pair (exactly 1 fixture)

| teacher      | student            |
|--------------|--------------------|
| `api-claude` | `bitnet-3B-local`  |

Contract: both non-empty AND distinct.

### σ-filtered distillation (exactly 4 fixtures, τ_learn = 0.25)

Rule: `σ_teacher ≤ τ_learn → LEARN` else SKIP (the
teacher was not confident enough, so the student does
not imitate).  Both branches fire.

| query_id | σ_teacher | decision |
|---------:|----------:|:--------:|
|  0       | 0.09      | LEARN    |
|  1       | 0.20      | LEARN    |
|  2       | 0.36      | SKIP     |
|  3       | 0.62      | SKIP     |

### Domain adaptation (exactly 3 fixtures, τ_domain = 0.30)

Rule: `σ_domain ≤ τ_domain → LOCAL_ONLY` else API.
Both branches fire.

| name     | σ_domain | route       |
|----------|---------:|:-----------:|
| `law`    | 0.18     | LOCAL_ONLY  |
| `code`   | 0.27     | LOCAL_ONLY  |
| `medical`| 0.44     | API         |

### Sovereign trajectory (exactly 4 checkpoints)

Contract: shares sum to 1; `api_share` strictly
decreasing; `local_share` strictly increasing; cost
strictly decreasing; `api_share[0] ≥ 0.75` and
`api_share[-1] ≤ 0.10`.

| checkpoint | api_share | local_share | cost (€/mo) |
|------------|----------:|------------:|------------:|
| `month_0`  | 0.80      | 0.20        | 200         |
| `month_1`  | 0.60      | 0.40        | 140         |
| `month_3`  | 0.30      | 0.70        |  60         |
| `month_12` | 0.05      | 0.95        |  20         |

### σ_distill

```
σ_distill = 1 − (pair_ok +
                 filter_rows_ok + filter_both_ok +
                 domain_rows_ok + domain_both_ok +
                 trajectory_rows_ok + traj_shares_ok +
                 traj_monotone_api_ok +
                 traj_monotone_local_ok +
                 traj_monotone_cost_ok +
                 traj_anchors_ok) /
                (1 + 4 + 1 + 3 + 1 + 4 + 1 + 1 + 1 + 1 + 1)
```

v0 requires `σ_distill == 0.0`.

## Merge-gate contract

`bash benchmarks/v277/check_v277_distill_runtime_live.sh`

- self-test PASSES
- Pair canonical (api-claude / bitnet-3B-local) and
  distinct
- 4 filter rows; LEARN iff σ_teacher ≤ 0.25; both
  branches fire
- 3 domains canonical (law / code / medical); route
  matches σ_domain cascade; both branches fire
- 4 trajectory checkpoints; shares sum to 1; api↓
  local↑ cost↓ strictly; anchors (≥0.75 at t0, ≤0.10
  at t12)
- `σ_distill ∈ [0, 1]` AND `σ_distill == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed pair / filter / domain /
  trajectory manifest with FNV-1a chain.
- **v277.1 (named, not implemented)** — live teacher /
  student loop wired into v262, real σ-filter on every
  teacher trace, domain router reading live σ_domain,
  measured monthly API share on a user's workload.

## Honest claims

- **Is:** a typed, falsifiable σ-distillation manifest
  where pair wiring, σ-filter branches, domain routing,
  and the sovereign trajectory (api↓, local↑, cost↓)
  are merge-gate predicates.
- **Is not:** a running distillation loop.  v277.1 is
  where the trajectory meets real API bills and a live
  BitNet.  This v0 does not compute actual cost savings
  beyond the typed manifest.

---

*Spektre Labs · Creation OS · 2026*

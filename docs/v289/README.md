# v289 — σ-Ruin-Value

**Graceful degradation: each layer falls away without catastrophe.**

Speer's ruin-value thesis: a building should fall apart beautifully
— every stage of collapse still has standing structure.  v289 types
the same discipline for Creation OS.  Every named subsystem can be
removed and the survivors keep working; the fallback cascade has
four independently-viable tiers; the σ-log always survives a crash;
five seed kernels are enough to regrow the whole stack from
wreckage.

## σ innovation (what σ adds here)

> **The bottom of the fallback cascade is pure σ-gate + BitNet —
> the still-beautiful ruin of last resort.**  σ is the signal that
> survives when everything else has failed, because it requires no
> external model to produce.

## v0 manifests

Enumerated in [`src/v289/ruin_value.h`](../../src/v289/ruin_value.h);
pinned by
[`benchmarks/v289/check_v289_ruin_value_graceful_degradation.sh`](../../benchmarks/v289/check_v289_ruin_value_graceful_degradation.sh).

### 1. Kernel removals (exactly 4 canonical rows)

| kernel_removed | survivor         |
|----------------|------------------|
| `v267_mamba`   | `transformer`    |
| `v260_engram`  | `local_memory`   |
| `v275_ttt`     | `frozen_weights` |
| `v262_hybrid`  | `direct_kernel`  |

Every `survivor_still_works == true`; all removals distinct.

### 2. Fallback cascade (exactly 4 canonical tiers, cost decreasing)

| tier | name                 | resource_cost | standalone_viable |
|------|----------------------|---------------|-------------------|
| 1    | `hybrid_engine`      | 1.00          | `true`            |
| 2    | `transformer_only`   | 0.60          | `true`            |
| 3    | `bitnet_plus_sigma`  | 0.25          | `true`            |
| 4    | `pure_sigma_gate`    | 0.05          | `true`            |

Contract: `tier_id` permutation [1..4]; every tier standalone-
viable; `resource_cost` strictly monotonically decreasing.

### 3. Data preservation (exactly 3 canonical guarantees)

`sigma_log_persisted · atomic_write_wal ·
last_measurement_recoverable`, each `guaranteed == true`.

### 4. Rebuild from ruin (exactly 3 canonical steps)

| step_order | step_name            | possible |
|------------|----------------------|----------|
| 1          | `read_sigma_log`     | `true`   |
| 2          | `restore_last_state` | `true`   |
| 3          | `resume_not_restart` | `true`   |

Scalar: `seed_kernels_required == 5` — the v229 seed bundle is
enough to reconstitute the full stack.

### σ_ruin (surface hygiene)

```
σ_ruin = 1 −
  (removal_rows_ok + removal_all_survive_ok +
   removal_distinct_ok + cascade_rows_ok +
   cascade_tier_permutation_ok + cascade_all_viable_ok +
   cascade_cost_monotone_ok + preservation_rows_ok +
   preservation_all_guaranteed_ok + rebuild_rows_ok +
   rebuild_order_ok + rebuild_all_possible_ok +
   seed_required_ok) /
  (4 + 1 + 1 + 4 + 1 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 1)
```

v0 requires `σ_ruin == 0.0`.

## Merge-gate contracts

- 4 removal rows; all survivors still work; all distinct.
- 4 cascade tiers; permutation [1..4]; all viable; costs strictly
  decreasing — merge condition "3 kerrosta putoaa, ydin toimii" is
  satisfied by tiers 2/3/4 still standing when tier 1 falls.
- 3 preservation guarantees; all hold.
- 3 rebuild steps; order permutation; all possible.
- `seed_kernels_required == 5`.
- `σ_ruin ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the removal / cascade / preservation /
  rebuild contracts as predicates.  No live crash harness, no WAL-
  backed σ-log runtime.
- **v289.1 (named, not implemented)** is a live crash-recovery
  harness exercising each cascade tier under fault injection, a
  WAL-backed σ-log that survives SIGKILL mid-write, and an
  executable `cos rebuild` that demonstrates resume (not restart)
  from the last σ-log entry.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

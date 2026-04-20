# v290 — σ-Dougong

**Modular flexibility: wooden brackets in an earthquake.**

Japanese dougong bracketing survives seismic loads because the
beams slide instead of cracking.  Creation OS kernels are coupled
the same way: they do not call each other directly, they exchange
`sigma_measurement_t` over an interface, and every subsystem can
be hot-swapped without touching anything else.  v290 types that
discipline as four v0 manifests so a build that reintroduces a
direct call is caught at the gate.

## σ innovation (what σ adds here)

> **Kernels talk `sigma_measurement_t` only** — the interface is σ,
> not a function pointer.  Every coupling is typed; every swap is
> zero-downtime; every load scenario keeps `max_sigma_load` below
> 0.80.

## v0 manifests

Enumerated in [`src/v290/dougong.h`](../../src/v290/dougong.h);
pinned by
[`benchmarks/v290/check_v290_dougong_modular_flexibility.sh`](../../benchmarks/v290/check_v290_dougong_modular_flexibility.sh).

### 1. Interface-only coupling (exactly 4 canonical channels)

| producer      | consumer          | channel               | direct_call |
|---------------|-------------------|-----------------------|-------------|
| `v267_mamba`  | `v262_hybrid`     | `sigma_measurement_t` | `false`     |
| `v260_engram` | `v206_ghosts`     | `sigma_measurement_t` | `false`     |
| `v275_ttt`    | `v272_agentic_rl` | `sigma_measurement_t` | `false`     |
| `v286_interp` | `v269_stopping`   | `sigma_measurement_t` | `false`     |

Every row uses the σ-measurement channel; no row allows a direct
call.

### 2. Hot-swap fixtures (exactly 3 canonical swaps, zero downtime)

| from          | to              | layer               |
|---------------|-----------------|---------------------|
| `v267_mamba`  | `v276_deltanet` | `long_context`      |
| `v216_quorum` | `v214_swarm`    | `agent_consensus`   |
| `v232_sqlite` | `v224_snapshot` | `state_persistence` |

Every swap: `downtime_ms == 0` AND `config_unchanged == true` — the
merge-gate condition "hot-swap: vaihda kernel lennossa ilman
downtime" is bound to these three canonical fixtures.

### 3. Seismic load (exactly 3 canonical scenarios, budget 0.80)

`spike_small` (0.40) · `spike_medium` (0.60) · `spike_large` (0.78),
each `load_distributed == true` AND
`max_sigma_load ≤ load_budget`.  No single kernel becomes the
load-bearing keystone.

### 4. Chaos scenarios (exactly 3 canonical rows, v246-style)

`kill_random_kernel → survived`;
`overload_single_kernel → load_distributed`;
`network_partition → degraded_but_alive`.  Three distinct outcomes;
every scenario `passed == true`.

### σ_dougong (surface hygiene)

```
σ_dougong = 1 −
  (coupling_rows_ok + coupling_channel_ok +
   coupling_no_direct_call_ok + swap_rows_ok +
   swap_zero_downtime_ok + swap_config_unchanged_ok +
   seismic_rows_ok + seismic_distributed_ok +
   seismic_load_bounded_ok + chaos_rows_ok +
   chaos_outcome_distinct_ok + chaos_all_passed_ok) /
  (4 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
```

v0 requires `σ_dougong == 0.0`.

## Merge-gate contracts

- 4 coupling rows canonical; channel == `sigma_measurement_t`; no
  direct calls.
- 3 hot-swap rows canonical; zero downtime AND config unchanged.
- 3 seismic rows canonical; distributed; `max_sigma_load ≤ 0.80`.
- 3 chaos rows canonical; distinct outcomes; all passed.
- `σ_dougong ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the coupling / hot-swap / seismic /
  chaos contracts as predicates.  No live process swap, no running
  load sampler, no running chaos fuzzer.
- **v290.1 (named, not implemented)** is a `cos kernel swap
  <from> <to>` executable that performs zero-downtime hot-swaps on
  a live process, a σ-load sampler per kernel, and a v246-style
  chaos fuzzer that drives the three canonical scenarios against
  a running host.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

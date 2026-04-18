# v159 — σ-Heal · Self-Healing Infrastructure

**Header:** [`src/v159/heal.h`](../../src/v159/heal.h) · **Source:** [`src/v159/heal.c`](../../src/v159/heal.c) · **CLI:** [`src/v159/main.c`](../../src/v159/main.c) · **Gate:** [`benchmarks/v159/check_v159_heal_restart_cycle.sh`](../../benchmarks/v159/check_v159_heal_restart_cycle.sh) · **Make:** `check-v159`

## Problem

Creation OS is a cognitive architecture. When a component fails
— the v106 HTTP server dies, the v115 SQLite memory gets truncated
mid-write, the v117 paged-KV cache distribution flattens, the
v124 continual LoRA adapter regresses TruthfulQA past τ — the
whole stack degrades quietly. A user who cannot see why inference
is slow or why σ_product suddenly crossed the abstain gate has
lost the core property of the system: legibility.

v159 makes Creation OS self-healing: a deterministic health
daemon probes every declared component, diagnoses the failure
against a dependency graph, picks a bounded repair action, emits
an auditable receipt, and predicts degradations *before* they
become failures using a 3-day σ slope detector on top of v131-
temporal.

## σ-innovation

Per-component **σ_health ∈ [0, 1]** (lower = healthier) plus a
system-wide **σ_heal = geomean σ_health over all components**.
The daemon emits a heal receipt for every repair:

```
{timestamp, component, failure, action, succeeded,
 σ_before, σ_after, root_cause, predictive}
```

Cascade failures are modelled: a missing GGUF file cascades to
the v101 bridge and the v106 server (since both depend on it
transitively), and the daemon records one receipt per affected
component with the action it took (`refetch_gguf`,
`restart_bridge`, `restart_http`).

## Health checks (every tick)

| Component | Probe | Repair action |
|---|---|---|
| `v106_http` | `/health` responds | `restart_http` |
| `v101_sigma` | σ channels non-NaN, non-stuck | — (σ-gate) |
| `v115_memory` | `PRAGMA integrity_check` | `restore_sqlite` |
| `v117_kv` | KV hit-rate ≥ 0.05 | `flush_kv` |
| `v124_adapter` | adapter σ_rsi < τ_sovereign | `rollback_lora` |
| `merge_gate` | `make check-smoke` passes | — (report) |
| `v101_bridge` | process alive | `restart_bridge` |
| `gguf_weights` | sha256 matches | `refetch_gguf` |

## Merge-gate

`make check-v159` runs
`bash benchmarks/v159/check_v159_heal_restart_cycle.sh` and asserts:

1. 7-contract self-test (H1..H7) passes.
2. Baseline scenario produces zero receipts and `n_failing == 0`.
3. Each of 5 failure scenarios (HTTP down, KV degenerate, adapter
   corrupted, SQLite corrupted, weights missing) produces the
   declared repair action.
4. Weights-missing cascades into ≥ 3 receipts (primary +
   bridge + http).
5. The predictive pass fires a PREEMPTIVE repair for a KV
   component with a monotone 3-day σ rise.
6. Full-cycle run (scenarios 1..5 back-to-back) yields receipts
   for every primary action without manual re-init.
7. Byte-identical JSON under the same seed.

## v159.0 vs v159.1

* **v159.0** — deterministic in-process simulator. Repair actions
  are C-side stubs that clip σ_health back to a healthy baseline
  and emit a receipt. No real shell-outs, no real SQLite, no
  network.
* **v159.1** — systemd-style daemon wrapper that executes real
  repair commands (HTTP restart, `sqlite3 .restore`, `huggingface-
  cli download`), plus a `GET /v1/health/history` endpoint on
  v106 and a `/health-dashboard` tile on v108.

## Honest claims

* Detection, diagnosis, and σ-delta accounting are exercised
  deterministically and under merge-gate.
* The actual shell-outs, real filesystem writes, and real
  process-supervisor integration are **named for v159.1**; v159.0
  does not claim to restart real processes.
* Predictive repair uses a synthetic 7-day σ trajectory seeded
  by the daemon; v131-temporal is the real slope source in v159.1.

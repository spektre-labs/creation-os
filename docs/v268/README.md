# v268 — σ-Continuous-Batch (`docs/v268/`)

σ-priority continuous batching.  Requests stream into
the serving engine without waiting for static batches;
priority is driven by σ_difficulty (low σ first),
preemption by σ_urgency, batch size by σ_load, and
engine choice by σ_cost.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### σ-priority queue (exactly 6 requests)

Rule: `priority_slot` is the **ascending** order of
`σ_difficulty` (low σ → fast path, served first).

| req           | σ_difficulty | engine          | slot |
|---------------|-------------:|-----------------|-----:|
| `req_greet`   | 0.05         | `local_fast`    | 1    |
| `req_retr`    | 0.12         | `local_fact`    | 2    |
| `req_sum`     | 0.24         | `local_fast`    | 3    |
| `req_code`    | 0.38         | `local_heavy`   | 4    |
| `req_reason`  | 0.56         | `local_heavy`   | 5    |
| `req_proof`   | 0.78         | `cloud_claude`  | 6    |

### Preemption (exactly 2 scenarios)

Rule: `preempted == (σ_urgency_arrival > σ_urgency_incumbent)`.
Both true and false outcomes must fire.

| scenario        | σ_inc | σ_arr | preempted | winner          |
|-----------------|------:|------:|:---------:|-----------------|
| `hot_arrival`   | 0.20  | 0.85  | true      | `req_urgent`    |
| `equal_arrival` | 0.45  | 0.45  | false     | `req_incumbent` |

### Adaptive batch size (exactly 3 load levels)

`batch_size` is strictly monotone-increasing in
`σ_load` — low load favours latency (small batch),
high load favours throughput (large batch).

| level   | σ_load | batch_size |
|---------|-------:|-----------:|
| low     | 0.15   | 4          |
| medium  | 0.45   | 16         |
| high    | 0.80   | 64         |

### Cost-aware routing (exactly 2 scenarios)

| scenario      | engine          | cost_eur | σ_cost |
|---------------|-----------------|---------:|-------:|
| `local_free`  | `local_fast`    | 0.00     | 0.00   |
| `api_paid`    | `cloud_claude`  | 3.80     | 0.42   |

Contract: `total_local_eur < total_api_eur`.

### σ_continuous_batch

```
σ_continuous_batch = 1 −
  (queue_ok + queue_order_ok + preempt_ok +
   preempt_branches_ok + batch_rows_ok +
   batch_monotone_ok + cost_rows_ok +
   cost_total_ok) /
  (6 + 1 + 2 + 1 + 3 + 1 + 2 + 1)
```

v0 requires `σ_continuous_batch == 0.0`.

## Merge-gate contract

`bash benchmarks/v268/check_v268_continuous_batch_priority.sh`

- self-test PASSES
- 6 queue rows; `priority_slot` permutation of [1..6]
  AND matches argsort(+σ_difficulty); `queue_order_ok`
- 2 preemption rows; both true AND false outcomes fire
- 3 load levels canonical (low, medium, high);
  `σ_load` AND `batch_size` strictly ascending;
  `batch_monotone_ok`
- 2 cost scenarios; `total_local_eur < total_api_eur`
- `σ_continuous_batch ∈ [0, 1]` AND
  `σ_continuous_batch == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed queue / preempt / load /
  cost manifest with FNV-1a chain.
- **v268.1 (named, not implemented)** — live queue
  wired to v262 hybrid engine running real preemption,
  measured latency + throughput under load, cost
  tracker emitting real €/month figures.

## Honest claims

- **Is:** a typed, falsifiable continuous-batch
  manifest where priority ordering, preemption
  outcomes, batch-size monotonicity, and local < api
  cost are merge-gate predicates.
- **Is not:** a live scheduler running preemptive
  inference.  v268.1 is where the manifest drives
  real load.

---

*Spektre Labs · Creation OS · 2026*

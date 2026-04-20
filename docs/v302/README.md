# v302 σ-Green — energy-aware inference

> **v0 scope.** v302 types four v0 manifests — a
> σ-aware compute budget, carbon-aware scheduling, an
> abstain-as-savings ledger, and a joules-per-*reliable*-
> token metric — and asserts that every row and every
> formula is as enumerated below. It does **not** plug
> into a real grid carbon feed; that is the v302.1
> follow-up. Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

AI inference is projected to cross **1000 TWh / year**,
with ~60% spent on inference. A σ-gate that abstains
20% of the time saves 20% of the energy of those 20% of
calls — **abstention is green by construction**.

v302 also introduces a new metric: **joules per
reliable token**, not joules per token. If 30% of your
output is noise, 30% of your energy is wasted. A
σ-filter that drops noisy tokens lowers
`J / reliable_token` even if total J is unchanged.

## v0 manifest contracts

### 1. σ-aware compute budget (3 rows)

| difficulty | σ_difficulty | model_tier | energy_j |
|------------|--------------|------------|----------|
| `easy`     | 0.10         | SMALL      | 0.5      |
| `medium`   | 0.40         | MID        | 2.0      |
| `hard`     | 0.80         | LARGE      | 8.0      |

**Contract.** σ strictly ↑; energy ↑; 3 DISTINCT model
tiers.

### 2. Carbon-aware scheduling (3 rows)

| label               | urgency | grid  | processed |
|---------------------|---------|-------|-----------|
| `urgent_green`      | HIGH    | GREEN | true      |
| `low_urgency_brown` | LOW     | BROWN | false     |
| `urgent_brown`      | HIGH    | BROWN | true      |

**Contract.** `processed = (urgency==HIGH) OR
(grid==GREEN)` on every row; both processed branches
fire — latency wins when it must, green wins when it
can.

### 3. Abstain = energy savings (3 rows)

| regime              | total | abstained | energy_j | saved_ratio |
|---------------------|-------|-----------|----------|-------------|
| `baseline`          | 1000  | 0         | 100      | 0.00        |
| `sigma_gated_light` | 1000  | 200       | 80       | 0.20        |
| `sigma_gated_heavy` | 1000  | 500       | 50       | 0.50        |

**Contract.** `saved_ratio = abstained/total` within
1e-3; `saved_ratio` strictly ↑; `energy_j` strictly ↓.

### 4. Joules per reliable token (3 rows)

| regime       | energy_j | reliable | noisy | J/reliable |
|--------------|----------|----------|-------|------------|
| `unfiltered` | 100      | 700      | 300   | 0.1429     |
| `soft_filter`| 80       | 700      | 0     | 0.1143     |
| `hard_filter`| 70       | 700      | 0     | 0.1000     |

**Contract.** `j_per_reliable = energy_j / reliable`
within 1e-3; strictly ↓ across the 3 rows.

## σ_green — surface hygiene

```
σ_green = 1 − (
    budget_rows_ok + budget_order_ok +
      budget_distinct_ok +
    sched_rows_ok + sched_rule_ok +
      sched_polarity_ok +
    save_rows_ok + save_ratio_ok +
      save_order_ok + save_energy_decrease_ok +
    jpt_rows_ok + jpt_formula_ok +
      jpt_decrease_ok
) / 20
```

v0 requires `σ_green == 0.0`.

## Merge-gate contracts

`make check-v302-green-energy-aware` asserts:

1. 3 canonical budget tiers; σ ↑, J ↑, 3 distinct tiers.
2. 3 canonical schedule rows; rule holds everywhere;
   both processed branches fire.
3. 3 canonical savings rows; `saved_ratio =
   abstained/total`; saved ↑; energy ↓.
4. 3 canonical J/reliable regimes; formula holds;
   strictly ↓.
5. `σ_green ∈ [0, 1]` AND `σ_green == 0.0`.
6. Deterministic output on repeat runs.

## v0 vs v302.1

- **v0 (this kernel):** canonical ledger rows + the
  new metric, asserted.
- **v302.1 (named, not implemented):** live
  carbon-intensity feeds from a national grid API
  gated by v293 hagia-sofia continuous-use, and a
  per-deployment `J_per_reliable_token` receipt emitted
  by the v282 σ-agent runtime.

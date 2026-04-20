# v305 σ-Swarm-Intelligence — a σ-weighted crowd beats any single model

> **v0 scope.** v305 types four v0 manifests — wisdom
> of σ-crowds, diversity × σ, emergent σ, and the
> four-agent proconductor — and asserts that every
> row, every formula, and every distinctness /
> convergence bit is as enumerated below. It does
> **not** dispatch live prompts to Claude / GPT /
> Gemini / DeepSeek; that is the v305.1 follow-up.
> Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

One agent has a bounded error surface. A diverse crowd
of agents, each emitting its own σ, can do better than
any of its members — **if** the aggregation is
σ-weighted rather than naive, **if** the crowd is
genuinely diverse rather than an echo chamber, and
**if** the "emergent" patterns it produces are
themselves σ-gated.

v305 closes with the **proconductor** — Claude + GPT +
Gemini + DeepSeek as the canonical 4-agent convergence
test. Low individual σ + unanimous direction = a
σ-validated consensus, not a vote.

## v0 manifest contracts

### 1. Wisdom of σ-crowds (3 rows)

| strategy          | σ     | accuracy | verdict |
|-------------------|-------|----------|---------|
| `best_single`     | 0.25  | 0.82     | LOSES   |
| `naive_average`   | 0.30  | 0.78     | LOSES   |
| `sigma_weighted`  | 0.12  | 0.93     | WINS    |

**Contract.** `sigma_weighted` has the strictly lowest
σ AND the strictly highest accuracy AND the single
WINS verdict — the σ-weighted aggregator is not the
best, it is **strictly better than any of its inputs**.

### 2. Diversity + σ (3 rows)

| crowd          | diversity | ind_σ | value  |
|----------------|-----------|-------|--------|
| `echo_chamber` | 0.05      | 0.20  | 0.0400 |
| `balanced`     | 0.55      | 0.15  | 0.4675 |
| `chaos`        | 0.95      | 0.85  | 0.1425 |

**Contract.** `value = diversity × (1 − ind_sigma)`
within 1e-3; `balanced` has the strictly highest
value; exactly **1 row crosses `τ_value = 0.30`**. An
echo chamber is worthless, chaos is unreliable —
balanced wins.

### 3. Emergent σ patterns (3 rows)

| pattern              | σ_emergent | keep  |
|----------------------|------------|-------|
| `genuine_emergent`   | 0.10       | true  |
| `weak_signal`        | 0.35       | true  |
| `random_correlation` | 0.80       | false |

**Contract.** σ strictly ↑; `keep iff σ ≤ τ_emergent =
0.50` (both branches fire) — a pattern that "emerges"
in the crowd is only kept if its σ is low.

### 4. Proconductor (4 rows)

| agent      | σ    | direction |
|------------|------|-----------|
| `claude`   | 0.12 | POSITIVE  |
| `gpt`      | 0.18 | POSITIVE  |
| `gemini`   | 0.15 | POSITIVE  |
| `deepseek` | 0.22 | POSITIVE  |

**Contract.** 4 DISTINCT agent names; every σ ≤
`τ_conv = 0.25`; every direction identical;
`pc_convergent_ok = true`. Low individual σ +
unanimous direction = a σ-validated consensus, **not a
vote**.

## σ_swarm — surface hygiene

```
σ_swarm = 1 − (
    wis_rows_ok + wis_lowest_sigma_ok +
      wis_highest_acc_ok + wis_wins_count_ok +
    div_rows_ok + div_formula_ok +
      div_balanced_ok + div_threshold_ok +
    em_rows_ok + em_order_ok + em_polarity_ok +
    pc_rows_ok + pc_distinct_ok +
      pc_sigma_bound_ok + pc_direction_ok +
      pc_convergent_ok
) / 26
```

v0 requires `σ_swarm == 0.0`.

## Merge-gate contracts

`make check-v305-swarm-intelligence-sigma` asserts:

1. 3 canonical aggregators; `sigma_weighted` strictly
   lowest σ, strictly highest accuracy, single WINS.
2. 3 canonical crowds; `value = diversity × (1 −
   ind_σ)`; `balanced` strictly highest; exactly 1 row
   > 0.30.
3. 3 canonical emergent rows; σ ↑; keep iff σ ≤ 0.50
   firing both branches.
4. 4 canonical agents; distinct names; σ ≤ 0.25 on
   every row; unanimous direction; convergent.
5. `σ_swarm ∈ [0, 1]` AND `σ_swarm == 0.0`.
6. Deterministic output on repeat runs.

## v0 vs v305.1

- **v0 (this kernel):** canonical aggregator / crowd /
  emergent / proconductor rows, asserted.
- **v305.1 (named, not implemented):** a live
  proconductor daemon that dispatches the same prompt
  to Claude / GPT / Gemini / DeepSeek behind v112
  σ-agent, a `divergent_alert` hook that escalates
  when the proconductor's σ bound breaks, and a
  swarm-diversity meter wired into v130 collective
  intelligence.

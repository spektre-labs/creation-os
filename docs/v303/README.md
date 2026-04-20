# v303 σ-Governance — σ on organisations

> **v0 scope.** v303 types four v0 manifests —
> decision σ, meeting σ, communication σ, and an
> institutional `K(t)` — and asserts that every row,
> every formula, and every three-state polarity is as
> enumerated below. It does **not** ship a live OKR
> crawler; the plumbing is the v303.1 follow-up. Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

v199..v203 introduced the social layer. v303 walks the
same discipline into a company:

- a **decision** has a σ;
- a **meeting** has a σ;
- a **message** has a σ;
- and the institution as a whole has a coherence
  `K(t) = ρ · I_φ · F` that must stay above `K_crit =
  0.127` or collapse is imminent.

v303 is the v0 manifest for those four.

## v0 manifest contracts

### 1. Decision σ (3 rows)

| label                         | σ_decision | verdict    |
|-------------------------------|------------|------------|
| `strategy_matches_ops`        | 0.05       | COHERENT   |
| `strategy_partially_realised` | 0.35       | DEGRADED   |
| `strategy_ignored`            | 0.75       | INCOHERENT |

**Contract.** σ strictly ↑; 3 DISTINCT verdicts.

### 2. Meeting σ (3 rows)

| label             | made | realised | σ_meeting |
|-------------------|------|----------|-----------|
| `meeting_perfect` | 10   | 10       | 0.00      |
| `meeting_quarter` | 8    | 6        | 0.25      |
| `meeting_noise`   | 10   | 3        | 0.70      |

**Contract.** `σ_meeting == 1 − realised/made` within
1e-3; σ strictly ↑. A meeting that generates 10
decisions but realises 3 of them has a σ of 0.7 — **70%
of that meeting was noise**.

### 3. Communication σ (3 rows)

| channel          | σ_comm | clear |
|------------------|--------|-------|
| `clear_message`  | 0.10   | true  |
| `slightly_vague` | 0.35   | true  |
| `highly_vague`   | 0.80   | false |

**Contract.** σ strictly ↑; `clear iff σ_comm ≤ τ_comm
= 0.50` (both branches fire).

### 4. Institutional K(t) (3 rows)

| org              | ρ    | I_φ  | F    | K     | status   |
|------------------|------|------|------|-------|----------|
| `healthy_org`    | 0.85 | 0.60 | 0.70 | 0.357 | VIABLE   |
| `warning_org`    | 0.50 | 0.50 | 0.60 | 0.150 | WARNING  |
| `collapsing_org` | 0.30 | 0.40 | 0.50 | 0.060 | COLLAPSE |

**Contract.** `K = ρ · I_φ · F` within 1e-3; VIABLE iff
`K ≥ K_warn = 0.20`; COLLAPSE iff `K < K_crit = 0.127`;
WARNING iff `K_crit ≤ K < K_warn`. All three branches
fire.

## σ_gov — surface hygiene

```
σ_gov = 1 − (
    dec_rows_ok + dec_order_ok + dec_distinct_ok +
    mtg_rows_ok + mtg_formula_ok + mtg_order_ok +
    comm_rows_ok + comm_order_ok + comm_polarity_ok +
    kt_rows_ok + kt_formula_ok + kt_polarity_ok
) / 20
```

v0 requires `σ_gov == 0.0`.

## Merge-gate contracts

`make check-v303-governance-org-sigma` asserts:

1. 3 canonical decisions; σ ↑; 3 distinct verdicts.
2. 3 canonical meetings; `σ_meeting = 1 − realised/made`.
3. 3 canonical comm channels; σ ↑; `clear iff σ ≤ 0.50`
   firing both branches.
4. 3 canonical institutions; `K = ρ·I_φ·F`; VIABLE /
   WARNING / COLLAPSE all firing on `K_warn = 0.20` and
   `K_crit = 0.127`.
5. `σ_gov ∈ [0, 1]` AND `σ_gov == 0.0`.
6. Deterministic output on repeat runs.

## v0 vs v303.1

- **v0 (this kernel):** canonical decision / meeting /
  comm / institutional rows, asserted.
- **v303.1 (named, not implemented):** live OKR / KPI
  pipes feeding decision-σ and meeting-σ straight into
  the v244 observability plane, and an
  institutional-K dashboard that raises a v283
  constitutional-AI escalation when K crosses
  `K_warn`.

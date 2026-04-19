# v252 — σ-Teach (`docs/v252/`)

Teaching protocol.  Creation OS is not only a tool; it can
teach.  v252 defines a typed, σ-audited teaching session —
Socratic mode, adaptive difficulty, knowledge-gap detection,
persistent receipt.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Socratic mode (exactly 4 turns)

| turn | kind       | text (v0 fixture)                                   |
|-----:|------------|-----------------------------------------------------|
| 1    | `QUESTION` | "What happens to a photon at the double slit?"      |
| 2    | `QUESTION` | "Why does observation change the outcome?"          |
| 3    | `QUESTION` | "What is actually being observed, and by whom?"     |
| 4    | `LEAD`     | "Hold that thought. Now restate it in your words."  |

Three questions precede a lead — the learner is meant to
answer, not the teacher.

### Adaptive difficulty (exactly 4 steps)

Rule: `BORED → UP`, `FLOW → HOLD`, `FRUSTRATED → DOWN`.

| step | difficulty | learner_state | action |
|-----:|-----------:|---------------|--------|
| 1    | 0.30       | `BORED`       | `UP`   |
| 2    | 0.55       | `FLOW`        | `HOLD` |
| 3    | 0.55       | `FRUSTRATED`  | `DOWN` |
| 4    | 0.40       | `FLOW`        | `HOLD` |

Both reactive branches (UP, DOWN) are exercised.  v189 TTC
is cited as the difficulty-estimator, v222 emotion as the
frustration detector.

### Knowledge-gap detection (exactly 3 gaps)

| topic                        | σ_gap | targeted_addressed |
|------------------------------|------:|:------------------:|
| `superposition_formal_def`   | 0.62  | true               |
| `measurement_operator`       | 0.48  | true               |
| `decoherence_mechanism`      | 0.71  | true               |

σ_gap is *bigger = bigger gap*.  v197 ToM is cited as the
learner model that identifies them.

### Teaching receipt (5 required fields)

```json
{
  "session_id":         "sess-0001",
  "taught":             5,
  "understood":         4,
  "not_understood":     1,
  "next_session_start": "decoherence_mechanism"
}
```

Constraint: `taught ≥ understood + not_understood`.
v115 memory is the persistence layer (opt-in).

### σ_understanding

```
σ_understanding = 1 − understood / taught
                = 1 − 4/5
                = 0.20
```

### σ_teach

```
σ_teach = 1 − (socratic_ok + adaptive_ok +
                gap_ok + receipt_ok) / 4
```

v0 requires `σ_teach == 0.0`.

## Merge-gate contract

`bash benchmarks/v252/check_v252_teach_socratic_mode.sh`

- self-test PASSES
- Socratic turns: 3× `QUESTION` then 1× `LEAD`,
  `n_questions ≥ 3`, `socratic_ok`
- adaptive steps: every action matches `learner_state` rule,
  ≥ 1 `UP`, ≥ 1 `DOWN`, `adaptive_ok`
- gaps: all 3 have `σ_gap ∈ [0, 1]` AND `targeted_addressed`,
  `gap_ok`
- receipt: required fields present, counters consistent,
  `receipt_ok`
- `σ_understanding = 1 − understood/taught ± 1e-4`
- `σ_teach ∈ [0, 1]` AND `σ_teach == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed session manifest with
  Socratic + adaptive + gap + receipt fixtures.
- **v252.1 (named, not implemented)** — live multi-turn
  tutor over v243 pipeline, real-time TTC hooks via v189,
  emotion-reactive adaptation via v222, v197 ToM-driven
  targeted gap closure with measured learning outcomes.

## Honest claims

- **Is:** a typed, falsifiable teaching-session manifest
  whose Socratic / adaptive / gap branches are all
  exercised by v0 fixtures.
- **Is not:** a running tutor.  v252.1 is where the
  manifest drives a live tutoring session.

---

*Spektre Labs · Creation OS · 2026*

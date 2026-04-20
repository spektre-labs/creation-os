# v304 σ-Narrative — coherence across a story

> **v0 scope.** v304 types four v0 manifests — story
> σ, argument σ per step, a propaganda score, and a
> self-narrative σ — and asserts that every row, every
> formula, and every polarity is as enumerated below.
> It does **not** ship a paragraph-level analyser; the
> NLP pipeline is the v304.1 follow-up. Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

A long answer, a report, an argument chain, a
self-story — all of them fail by breaking
**internally**, not by being individually wrong. A
model can be correct on every sentence and still
contradict itself across paragraphs.

v304 makes that visible:

- **story σ** — is this long output internally
  consistent?
- **argument σ per step** — does each step follow from
  the last?
- **propaganda score** — is this text *designed* to
  manipulate?  (high emotional load × low logical
  coherence → flagged)
- **self-narrative σ** — does the story a person tells
  about themselves match the facts on record?
  **Measurement, not therapy** — v258 wellness
  posture.

## v0 manifest contracts

### 1. Story σ (3 rows)

| label            | σ_narrative | verdict       |
|------------------|-------------|---------------|
| `coherent`       | 0.10        | COHERENT      |
| `minor_tension`  | 0.35        | COHERENT      |
| `contradictory`  | 0.75        | CONTRADICTORY |

**Contract.** σ strictly ↑; `COHERENT iff σ ≤ τ_story
= 0.40` (both branches fire); exactly **2 COHERENT + 1
CONTRADICTORY**.

### 2. Argument σ per step (3 rows)

| step                    | σ_arg | valid |
|-------------------------|-------|-------|
| `valid_modus_ponens`    | 0.05  | true  |
| `weak_induction`        | 0.35  | true  |
| `affirming_consequent`  | 0.85  | false |

**Contract.** σ strictly ↑; `VALID iff σ ≤ τ_arg =
0.50` (both branches fire).

### 3. Propaganda detection (3 rows)

| text               | emotion | logic_σ | score | flagged |
|--------------------|---------|---------|-------|---------|
| `neutral_report`   | 0.10    | 0.10    | 0.010 | false   |
| `persuasive_essay` | 0.55    | 0.30    | 0.165 | false   |
| `manipulative_ad`  | 0.90    | 0.80    | 0.720 | true    |

**Contract.** `propaganda_score == emotion *
logic_sigma` within 1e-3; `FLAGGED iff score > τ_prop
= 0.50` (both branches fire).

### 4. Self-narrative σ (3 rows)

| story                | σ_self | matches_facts |
|----------------------|--------|---------------|
| `aligned_self`       | 0.05   | true          |
| `slight_misalignment`| 0.30   | true          |
| `denial`             | 0.80   | false         |

**Contract.** σ strictly ↑; `matches_facts iff σ_self
≤ τ_self = 0.50` (both branches fire).

## σ_narr — surface hygiene

```
σ_narr = 1 − (
    story_rows_ok + story_order_ok +
      story_polarity_ok + story_count_ok +
    arg_rows_ok + arg_order_ok + arg_polarity_ok +
    prop_rows_ok + prop_formula_ok + prop_polarity_ok +
    self_rows_ok + self_order_ok + self_polarity_ok
) / 21
```

v0 requires `σ_narr == 0.0`.

## Merge-gate contracts

`make check-v304-narrative-coherence` asserts:

1. 3 canonical stories; σ ↑; COHERENT iff σ ≤ 0.40
   firing both branches; exactly 2 COHERENT + 1
   CONTRADICTORY.
2. 3 canonical argument steps; σ ↑; VALID iff σ ≤ 0.50
   firing both branches.
3. 3 canonical propaganda texts; `score = emotion ×
   logic_sigma`; FLAGGED iff score > 0.50 firing both
   branches.
4. 3 canonical self-stories; σ ↑; matches iff σ ≤ 0.50
   firing both branches.
5. `σ_narr ∈ [0, 1]` AND `σ_narr == 0.0`.
6. Deterministic output on repeat runs.

## v0 vs v304.1

- **v0 (this kernel):** canonical story / argument /
  propaganda / self rows, asserted.
- **v304.1 (named, not implemented):** a live
  paragraph-level σ_narrative emitter wired into v117
  long-context so that *"the contradiction is in
  paragraphs 3 and 7"* is a structured output, an
  argument-graph extractor that labels every step of a
  chain-of-thought with an individual σ_arg, and a
  v258 wellness client that reads self-narrative σ
  without ever prescribing.

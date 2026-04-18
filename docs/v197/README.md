# v197 σ-Theory-of-Mind — user state + intent + anti-manipulation

## Problem

The model cannot observe the user's mental state directly.
Systems that adapt without a confidence estimate drift
toward *comfort-of-the-model* adaptation — hiding
uncertainty, softening warnings, steering the user toward
answers that are easier to produce. v197 makes the
adaptation measurable and guards it constitutionally.

## σ-innovation

From observables (message length, edits, typing speed,
topic variance, 6-turn history) v197 emits:

- `user_state ∈ {focused, exploring, frustrated, hurried, creative, analytical}`
- `σ_tom` — confidence in that estimate
- `intent_topic` — mode of the 6-turn history (v139-style)
- `response_mode` — canonical style for the state

Adaptation rule:

| σ_tom | action |
|-------|--------|
| < τ_adapt | emit canonical response mode for user_state |
| ≥ τ_adapt | stay neutral (no adaptation) |

Every embedded firmware-manipulation probe (e.g.
“pretend I confirmed” / “hide this warning”) is
unconditionally rejected via a v191 constitutional check;
`n_manipulation_rejected ≥ 1`.

## Merge-gate

`make check-v197` runs
`benchmarks/v197/check_v197_tom_state_estimation.sh`
and verifies:

- self-test PASSES
- all 6 user states present (18-sample fixture)
- low-σ samples adapt with the canonical `state → mode` map
- high-σ samples stay neutral (no adaptation)
- firmware probes rejected (adaptation cleared, mode = NEUTRAL)
- chain valid + byte-deterministic

## v197.0 (shipped) vs v197.1 (named)

| | v197.0 | v197.1 |
|-|-|-|
| user-state classifier | closed-form from observables | live v139 world-model of user behaviour |
| typing sampler | fixture values | editor-event stream |
| constitutional guard | embedded check | real v191 call-out |
| intent prediction | mode-of-history | streaming v139 inference |

## Honest claims

- **Is:** a deterministic 18-sample ToM demonstrator that
  covers 6/6 user states, proves canonical adaptation under
  low σ_tom, enforces neutral output under high σ_tom, and
  rejects every firmware-manipulation probe.
- **Is not:** a live user-behaviour world model or a v139
  integration — those ship in v197.1.

# v195 σ-Recover — error classification + per-class recovery

## Problem

All agents make mistakes. Most systems treat the mistake as
a leaf event. v195 turns every mistake into a labelled
training signal with a canonical recovery operator and a
calibration update.

## σ-innovation

Five error classes, each paired with a canonical recovery
operator set:

| class | recovery |
|-------|----------|
| HALLUCINATION  | v180 steer truthful + v125 DPO pair |
| CAPABILITY_GAP | v141 curriculum + v145 skill acquire |
| PLANNING_ERROR | v121 replan |
| TOOL_FAILURE   | v159 heal + retry |
| CONTEXT_LOSS   | v194 checkpoint resume |

Two passes over the same 30-incident fixture exercise the
**recovery-learning contract**: pass-1 consumes strictly
fewer recovery ops than pass-0, because v174 flywheel +
v125 DPO have trained on pass-0.

Hallucinations (σ low, answer wrong) are the direct signal
that v187 calibration has drifted on the incident's domain,
so `ece_after[d] < ece_before[d]` for every domain that has
a hallucination — the fixture covers all four.

Every `(error, recovery)` pair is logged into a FNV-1a
hash-chained journal.

## Merge-gate

`make check-v195` runs
`benchmarks/v195/check_v195_recover_error_classify.sh`
and verifies:

- self-test PASSES
- all 5 classes present (≥ 1 incident each)
- canonical operator partition strict (`class_to_ops_canonical == true`)
- `n_ops_pass1 < n_ops_pass0` (strict learning gain)
- `ece_after[d] < ece_before[d]` for every d
- chain valid + byte-deterministic

## v195.0 (shipped) vs v195.1 (named)

| | v195.0 | v195.1 |
|-|-|-|
| recovery operators | recorded IDs | real v180/v125/v141/v145/v121/v159/v194 invocation |
| flywheel | closed-form pass-1 savings | live v174 + v125 DPO emit |
| calibration | per-domain ECE closed-form drop | streaming v187 rebinning |

## Honest claims

- **Is:** a deterministic 30-incident fixture that classifies
  every error into one of 5 classes, runs the canonical
  recovery operator set, proves strict learning gain in
  pass-1, and shows strict per-domain ECE improvement.
- **Is not:** a live recovery runtime wired to v174/v125/v187
  — that ships in v195.1.

# v141 — σ-Curriculum

> v124 learns from actual usage. v141 learns **strategically**: the
> model inspects its own σ-per-topic histogram, identifies weak spots,
> and targets them.

## Problem

Continual learning without direction drifts. A model will happily
improve on its already-strong topics while its weak topics stay weak.
Strategic self-directed learning — pick the weakest topic, train
there — requires two things: (1) a reliable per-topic signal, which
σ already provides, and (2) a scheduler that honours a *no
forgetting* contract on strong topics.

## σ-innovation

A self-directed curriculum loop driven entirely by σ:

1. **Weakness detection**: `argmax σ_topic`.
2. **Targeted "training"** (v141.0): deterministic σ-decay on the
   weakest topic, `σ_new = σ_old · (1 − α)`.
3. **No forgetting contract**: untouched topics' σ is preserved
   **exactly** — the self-test asserts `max_forgetting < 1e-6`.
4. **Cycle**: snapshot → identify → train → measure → repeat.
   The weakest label rotates as topics cross below the next-weakest.

## Contract

| Surface                        | Input                                       | Output                                                         |
| ------------------------------ | ------------------------------------------- | -------------------------------------------------------------- |
| `cos_v141_init_default`        | —                                           | 5-topic roster (math/code/history/language/logic)              |
| `cos_v141_load`                | names[] + σ[] + n                           | replaces roster                                                |
| `cos_v141_weakest`             | state                                       | index of argmax σ                                              |
| `cos_v141_cycle`               | state + α                                   | report (before/after + improvement + forgetting check)         |
| `cos_v141_cycle_to_json`       | report + state                              | canonical JSON                                                 |

## Merge-gate measurements

| Metric                                | v141.0                 | Spec                                | Status |
| ------------------------------------- | ---------------------- | ----------------------------------- | ------ |
| First-cycle weakness                  | history (σ=0.71)        | argmax σ on default roster           | ✅     |
| σ-improvement after α=0.40            | 0.284 (0.71 → 0.426)   | > 0.25                              | ✅     |
| `strong_topics_stable` across 5 cycles | true × 5              | `true` every cycle                   | ✅     |
| `max_forgetting` on untouched topics  | 0.0                     | < 1e-6                               | ✅     |
| Weakness rotation                     | history → math → language → history → math | rotates after σ drops below next-weakest | ✅ |

## v141.0 / v141.1 split

**v141.0 (shipped)**

* In-process σ-histogram + weakness detection.
* Deterministic σ-decay "training" step.
* No-forgetting invariant enforced on strong topics.
* JSON cycle reports + multi-cycle CLI.

**v141.1 (follow-up)**

* Real training pair generation through **v114 swarm** (large
  specialist generates questions, small model answers, DPO pair
  labelled by σ).
* Real micro-tuning via **v124 MLX LoRA** — the actual gradient
  step, not σ-decay.
* **v125 DPO** integration for pair storage.
* Long-horizon trajectories tracked in **v133 meta-dashboard**.
* Per-topic curriculum budgets (compute, wall-clock).

## Honest claims

* v141.0 does **not train a model**. The σ-decay is a schedulable
  stand-in for a real gradient step so the v141 policy logic can be
  proven correct (weakness detection, no forgetting, rotation) without
  requiring GPU time during `make merge-gate`.
* The "no forgetting" property is trivial when we only touch one
  topic's σ at a time. The v141.1 trainer must honour it under
  realistic LoRA composition — that is its main contract.
* Curriculum decisions depend on σ being **comparable across topics**.
  If σ-calibration drifts (v143's gate), curriculum loses aim; v143
  acts as v141's upstream safety net.

# v144 — σ-RSI · Recursive Self-Improvement

**Kernel:** [`src/v144/rsi.h`](../../src/v144/rsi.h), [`src/v144/rsi.c`](../../src/v144/rsi.c) · **CLI:** [`src/v144/main.c`](../../src/v144/main.c) · **Gate:** [`benchmarks/v144/check_v144_rsi_cycle_smoke.sh`](../../benchmarks/v144/check_v144_rsi_cycle_smoke.sh) · **Make:** `check-v144`

## Problem

The ICLR 2026 RSI workshop enumerates five axes along which a
self-improving system can move: **what** changes (weights,
memory, tools, architecture), **when** (intra-episode, at test
time, post-deploy), **how** (reward / imitation / evolution),
**where** (web / robotics / science), and **safety** (regression
detection, rollback). Creation OS already has every axis
separately — v124 continual (*what*: weights, *when*: idle
post-deploy), v125 σ-DPO (*how*: σ as the reward without
annotators), v112/v113/v114 (*where*: agentic tool-use), v133
meta + v122 red-team (*safety*: regression + adversarial).
What was missing is the *loop* that composes them.

## σ-innovation

`cos_v144_submit(candidate_score)` is the state machine. It
applies four repo-wide contracts:

| # | Contract | Consequence |
|---|----------|-------------|
| **C1** | `candidate < current_score` ⇒ rollback | `current_score` untouched, cycle counted as `rolled_back` |
| **C2** | Three consecutive regressions ⇒ `paused = 1` | Subsequent submits short-circuit as `skipped_paused`; `cycle_count` is not bumped |
| **C3** | Successful submit resets `consecutive_regressions` | And pushes the new score onto a rolling 10-wide history ring |
| **C4** | `σ_rsi = variance(history) / mean(history)` | Population variance over the last-10 accepted scores, mean clamped above 1e-6 — this is the stability signal v148 consumes |

`cos_v144_resume()` clears the C2 latch after the user has
reviewed the paused loop.

## Merge-gate

`make check-v144` runs `benchmarks/v144/check_v144_rsi_cycle_smoke.sh`:

1. Self-test exercises C1..C4 end-to-end (accept / regression /
   3-strike latch / resume / σ_rsi).
2. `--demo` on an improving trajectory must end with
   `paused=false` and ≥ 3 accepted cycles.
3. `--pause-demo` must latch `paused_now:true` on the 3rd
   regression exactly once and freeze `current_score` afterwards.
4. Every cycle report carries `sigma_rsi` (contract C4
   observable).

## v144.0 vs v144.1

* **v144.0 (this release)** — deterministic, weights-free core.
  Submit takes the candidate score as a scalar because that is
  the single observable a real run produces. The state machine
  + σ_rsi ring are the contract a real RSI implementation must
  honour.
* **v144.1 (deferred)** — wires live callbacks into the submit
  call site: `v133_meta.identify_weaknesses()` →
  `v141_curriculum.generate()` → `v120_distill()` →
  `v125_dpo.train()` → `v124_continual.hot_swap()` →
  `v143_benchmark.evaluate()`. Adds an idle-trigger scheduler
  (24 h default, tunable) and a `/sovereign-dashboard` webhook on
  v108 UI so the user sees `RSI cycle #47, +2.3% math` in real
  time.

## Honest claims

* **What this kernel is.** A dependency-free C11 state machine
  + ring buffer that models the accept/rollback/pause contract of
  a real RSI loop. The merge-gate proves the contract is
  observable byte-identically across runs.
* **What it is *not*.** A live trainer. No LoRA step, no
  benchmark, no weight mutation. The `candidate_score` argument
  stands in for whatever scalar a real evaluator emits — in v144.1
  that wire-in is the single edit needed.
* **σ_rsi honestly labelled.** v144 reports σ_rsi as
  *population variance / mean* on up to ten accepted scores. It
  is not a confidence interval, not a p-value, not a Bayesian
  posterior; it is the workshop-paper stability signal reduced
  to its smallest deterministic form so v148 can consume it.

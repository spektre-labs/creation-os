# v147 — σ-Reflect · Deep Self-Reflection

**Kernel:** [`src/v147/reflect.h`](../../src/v147/reflect.h), [`src/v147/reflect.c`](../../src/v147/reflect.c) · **CLI:** [`src/v147/main.c`](../../src/v147/main.c) · **Gate:** [`benchmarks/v147/check_v147_reflect_thought_trace.sh`](../../benchmarks/v147/check_v147_reflect_thought_trace.sh) · **Make:** `check-v147`

## Problem

v133 meta measures *outcome* σ — the confidence at answer time.
But a long reasoning chain can be correct in the answer while
wrong in the middle, or wrong in the answer because one
intermediate step drifted. v147 goes one level deeper: it
records the **thought trace** from the v111.2 reasoning loop,
tags each intermediate step with its own σ_product, and asks
three structural questions.

## σ-innovation

| # | Question | Operator |
|---|----------|----------|
| **R1 Consistency** | Does the pure answer (no chain shown) agree with the chain answer? | `strcmp(pure, chain.final_answer) == 0` |
| **R2 Weakest link** | Which thought is the most uncertain? | argmax σ_product across `trace.thoughts` |
| **R3 RAIN rewind** | How far to rewind on self-reject? | ICLR 2024 — *Rewindable Auto-regressive INference* |

The RAIN depth function is the repo-wide contract:

| σ_weakest | Rewind depth | Interpretation |
|-----------|--------------|----------------|
| σ ≤ 0.30            | 1                 | Local fix |
| 0.30 < σ ≤ 0.60     | ⌈n/2⌉             | Mid-chain restart |
| σ > 0.60            | n                 | Full chain restart |

Divergence (R1 false) is reported alongside the weakest step
(R2) so the sovereign loop knows both *that* the chain went wrong
*and where to rewind from* (R3).

## Merge-gate

`make check-v147`:

1. Self-test covers all three RAIN depth buckets (local / mid /
   full) plus divergence detection plus empty-trace edge case.
2. `--demo` on the canonical 4-step trace (σ = 0.15, 0.22, **0.67**,
   0.18) must report:
   - `weakest_step = 2`
   - `weakest_sigma = 0.67`
   - `rewind_depth = 4` (full chain — σ > 0.60)
   - `consistency_ok = true` (pure = "42")
   - `rain_should_rewind = true`
3. `--divergence-demo` (pure = "WRONG") flips `consistency_ok`
   to `false`, sets `divergence_detected = true`, and the weakest
   step is still identified.

## v147.0 vs v147.1

* **v147.0** — deterministic, string-level. Thought traces are
  constructed by the caller; σ per step is an input. No model
  calls.
* **v147.1** — v111.2 reasoning loop pipes its step-by-step σ
  profile directly into `cos_v147_trace_t`; a pure-answer probe
  runs through the same v106 endpoint with `cot: false` for the
  R1 comparison; v125 DPO uses `weakest_step` as a preference
  signal (the "wrong move" token-for-token); the sovereign loop
  (v148) calls `cos_v147_rewind_depth()` to decide how far to
  unwind a failing plan.

## Honest claims

* **v147.0 does not invoke a model.** It is a pure function of
  `(trace, pure_answer)`. The structural analysis is all
  contract; the value of v147 is the *shape* of the question,
  not the number it returns.
* **Consistency is strict string equality.** v147.1 replaces it
  with a σ-bounded semantic match (same σ-aggregator as the rest
  of the stack). v147.0's stricter check is the safer failure
  mode — it *over*-reports divergence, never under-.
* **RAIN thresholds are repo-wide constants.** `0.30` and `0.60`
  match the σ-governance constants in v101/v105. If the wider
  stack retunes those, v147 inherits the retune automatically
  because it imports the same header.

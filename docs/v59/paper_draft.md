# σ-Budget: σ-Decomposed Adaptive Test-Time Compute Budgeting with a
# Branchless Four-Valued Decision Kernel

_Creation OS v59 — paper draft_

## Abstract

We introduce **σ-Budget**, a per-step adaptive test-time compute budget
controller for LLM reasoning that decomposes the uncertainty signal σ
into reducible (ε) and irreducible (α) components and produces a
**four-valued** decision tag —  `CONTINUE`,  `EARLY_EXIT`,  `EXPAND`,
`ABSTAIN` — via a branchless kernel in C11.  The decomposition makes
ABSTAIN possible on α-dominated problems, a decision no prior scalar-
signal policy (TAB, CoDE-Stop, LYNX, DTSR, DiffAdapt, Coda, AdaCtrl,
Risk-Control Budget Forcing) can produce faithfully.  We ship a
branchless NEON 4-accumulator kernel, a 69-test deterministic self-test
suite, and a microbench at 1.1–1.5 × 10⁸ decisions / s on an M-class
chip.

## 1. Problem

Q2 2026 consolidated around adaptive test-time compute budgeting as the
central lever for efficient reasoning.  The common move is to detect
"easy" problems and stop early.  Published methods agree on the shape
of the decision — binary stop/continue or length-selector — but differ
in the signal driving it: GRPO policies (TAB, AdaCtrl), confidence
dynamics (CoDE-Stop), learned probes (LYNX), reflection counters
(DTSR), entropy patterns (DiffAdapt), rollout difficulty (Coda),
distribution-free dual thresholds (Risk-Control Budget Forcing).

All of these signals are **scalar**.  None distinguishes reducible from
irreducible uncertainty.

**Consequence.**  A scalar policy wastes compute on α-dominated
problems — genuinely ambiguous, underspecified, or noisy tasks where
no amount of extra reasoning tokens will reduce σ.  Scalar policies
also lack a faithful abstain: to them, low-σ noise and low-σ consensus
look the same.

## 2. Signal

v34 Dirichlet / v55 σ₃ / v56 VPRM already emit per-step

    ε — reducible uncertainty (more compute helps)
    α — irreducible uncertainty (more compute does not help)

σ-Budget consumes ε, α, `answer_delta` (CoDE-Stop-style stability), and
`reflection_signal` (DTSR-style reflection-tag detector), and emits a
single byte tag from `{ CONTINUE, EARLY_EXIT, EXPAND, ABSTAIN }`.

## 3. Policy

```
readiness(step) = β · (1 − answer_delta)
                + γ · reflection_signal
                − α_ε · ε
                − δ   · α

σ_total(step)   = ε + α
α_frac(step)    = α / (ε + α)

if n ≥ budget_max_steps                                      → EARLY_EXIT
elif σ_total ≥ σ_high ∧ α_frac ≥ α_dom ∧ n ≥ budget_min       → ABSTAIN
elif σ_total ≥ σ_high ∧ α_frac <  α_dom                       → EXPAND
elif readiness ≥ τ_exit ∧ n ≥ budget_min                      → EARLY_EXIT
else                                                          → CONTINUE
```

All five branches are encoded as 0/1 lane masks AND-NOT cascaded;
the decision kernel contains no `if` on the hot path.

## 4. Kernel

- Scalar scoring: three FMAs per step (stability + reflection + ε + α).
- NEON 4-accumulator SoA reduction (.cursorrules item 5): four
  accumulators, three `vfmaq_f32` stages each, prefetch 16 lanes ahead.
- Branchless decision kernel (.cursorrules item 4): masks composed with
  `& | ~` and a four-way mux, priority encoded by AND-NOT layering.
- 64-byte aligned step buffers via `cos_v59_alloc_steps` + `v59_alloc64`
  helper (macOS requires size to be a multiple of alignment; the helper
  rounds up).

## 5. Correctness (M-tier)

69 deterministic self-tests cover:

1. Version + defaults + null-safety (11 tests)
2. Scoring monotonicity, reward/penalty directions, batch = scalar,
   NEON SoA = scalar, translation-invariance in `step_idx` (9 tests)
3. Decision kernel: all four outputs reachable, priority cascade, cap
   beats abstain, abstain beats exit-on-ready, below-min forces
   continue, deterministic, idempotent on stable history (19 tests)
4. Trace summary: null-safety, n=0 path, counts sum to n, totals match
   step-wise sum, final decision matches online kernel, stress-random
   invariants (13 tests)
5. Tags, allocator alignment, zeroing (10 tests)
6. End-to-end scenarios: easy problem exits, ambiguous abstains, hard-
   but-tractable expands, mixed input uses multiple tags (4 tests)

## 6. Microbench

Three-point sweep on an M-class chip (deterministic synthetic traces):

| N     | iters | ms / iter | decisions / s |
|-------|-------|-----------|---------------|
| 64    | 5000  | 0.0006    | 1.15 × 10⁸    |
| 512   | 1000  | 0.0046    | 1.11 × 10⁸    |
| 4096  | 200   | 0.0269    | 1.52 × 10⁸    |

At 100 M decisions / s the budget controller is effectively free even
at 10⁵-token reasoning traces — it does not occupy the inference
critical path.

## 7. Positioning

| System           | Signal          | Decisions                       |
|------------------|-----------------|---------------------------------|
| TAB              | GRPO            | {stop, continue}                |
| CoDE-Stop        | conf dynamics   | {stop, continue}                |
| LYNX             | probe scalar    | {exit, stay}                    |
| DTSR             | reflection cnt  | {stop, continue}                |
| DiffAdapt        | entropy         | {easy, normal, hard}            |
| Coda             | rollout diff.   | {compute gate}                  |
| AdaCtrl          | RL length pol.  | {length control}                |
| Risk-Control BF  | dual threshold  | {confident-stop, unsolvable}    |
| **v59**          | **σ = (ε, α)**  | **{CONT, EXIT, EXPAND, ABSTAIN}** |

v59 is the only policy that can **refuse to spend compute on
irreducible problems** because it is the only one that can name them.

## 8. Non-claims

- v59 does not claim better end-to-end accuracy-under-budget than any
  prior system.  End-to-end numbers are P-tier and require a specific
  upstream σ source (v34 / v55 / v56) and a specific base model.
- No Frama-C / TLA+ proof yet.  M-tier only.
- The σ signal itself is v34's; v59's novelty is **its decomposition
  as the budget-decision surface**.

## 9. Integration

v59 is registered as the `adaptive_compute_budget` slot in the v57
Verified Agent:

```
[M] adaptive_compute_budget    (owner: v59  target: make check-v59)
```

v53's σ-TAOR harness and v54's proconductor both consume v59's
decision tags upstream.

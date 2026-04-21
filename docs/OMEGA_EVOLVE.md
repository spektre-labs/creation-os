# `cos-evolve` — σ-guided parameter evolution (Ω-stack)

> **Scope discipline.**  This document describes a *scaffold* for
> recursive self-improvement with a σ-gate as the fitness function.
> It ships with a deterministic synthetic fixture and a
> self-contained correctness harness.  Applying it to the live
> kernels on real workloads — TruthfulQA 817, ARC, HellaSwag — is
> the next experimental campaign, not a shipped result.  No overnight
> numbers are asserted in this doc.  See
> [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).

## Why σ-gated evolution is different

AVO, AlphaEvolve, Karpathy-autoresearch, AccelOpt — 2026's flagship
self-improving systems — all share the same blind spot: a fitness
function that treats *wrong but fast* as an improvement.  Creation
OS refuses that trade.  **The σ-gate is the fitness function.**  A
mutation is kept iff a calibrated uncertainty score drops; nothing
else counts.

> Ω = argmin ∫σ dt   subject to   K_eff ≥ K_crit.

Everything below implements that equation.

## Subcommands

```
cos-evolve evolve          σ-guided (1+1)-ES on the σ-ensemble
                           weights + τ · Brier fitness · keep/revert
cos-evolve memory-top      top-N accepted mutations from the sqlite
                           opt_mutations ledger
cos-evolve memory-stats    aggregate ledger stats
cos-evolve calibrate-auto  τ-sweep minimising false_accept / false_reject
                           on a labeled σ-fixture (balanced or
                           risk-bounded mode)
cos-evolve discover        declarative hypothesis harness: JSONL of
                           (hypothesis, command, expected), append-only
                           verdicts to ~/.cos/evolve/discoveries.jsonl
cos-evolve omega           recursive evolve + calibrate loop (N rounds)
cos-evolve daemon          foreground watchdog loop — halts on
                           σ-envelope rise (rolling_min + ε over 3
                           consecutive iterations) or SIGINT/SIGTERM
cos-evolve demo            deterministic 40-row showcase end-to-end
cos-evolve self-test       unit tests for every primitive
```

All state persists under `$COS_EVOLVE_HOME` (default
`~/.cos/evolve`):

| file                   | purpose                                 |
|------------------------|-----------------------------------------|
| `params.json`          | current champion σ-ensemble weights + τ |
| `evolve_log.jsonl`     | append-only per-run summary             |
| `memory.db`            | sqlite opt-mutations ledger             |
| `discoveries.jsonl`    | hypothesis harness verdicts             |
| `omega_daemon.log`     | daemon heartbeat + halt reason          |
| `demo_fixture.jsonl`   | deterministic demo input                |

## Fitness in detail — why Brier, not accuracy

The tunable genome is the **σ-ensemble weights**
`(w_logprob, w_entropy, w_perplexity, w_consistency)` plus the σ-gate
threshold `τ`.  On a labeled fixture with raw four-signal rows, the
fitness is

```
brier = (1/N) · Σ  ( w·σ_i  −  (1 − correct_i) )²
```

i.e. the mean squared distance between the weighted σ-combined score
and the ideal target (0 for correct, 1 for incorrect).  For fixed
per-row signals, Brier is convex in `w`, so (1+1)-ES with elitist
selection has a known-good attractor.  That means:

1. The self-test on the internal synthetic fixture is deterministic
   and reproducible across hosts.
2. "Evolution improved brier" is a falsifiable claim, not a vibe.
3. The Brier scale maps cleanly onto the calibrated-τ regime that
   `cos calibrate --auto` optimises.

Plain accuracy would make the optimum discrete, flat, and
step-function-shaped — a hostile surface for Gaussian-step ES.

## Self-calibration (τ auto-tune)

`cos-evolve calibrate-auto` computes per-row σ_combined under the
current weights, then grid-sweeps τ on `[0, 1]` step `0.005`:

```
BALANCED      loss(τ) = α · false_accept(τ) + (1-α) · false_reject(τ)
RISK_BOUNDED  loss(τ) = false_reject(τ)    s.t. false_accept ≤ α
```

The selected τ is written back to `params.json`.  Risk-bounded mode
lands a direct conformal-style operating point without invoking the
PAC machinery — useful during evolution, before you re-run full
conformal calibration.

## Daemon watchdog

The daemon executes one `evolve` round per iteration and maintains a
rolling minimum of `brier`.  Three consecutive iterations whose best
score rises more than `rise_limit` above the rolling minimum trip a
**rollback to the pre-iteration snapshot** and a clean halt with
`reason=brier_rise`.  SIGINT / SIGTERM also halt cleanly
(`reason=signal`).  This is the "stop when σ says so" rule — the
piece no AVO-style system includes by default.

## Optimisation memory (sqlite)

Every mutation attempt — accepted or reverted — lands in
`memory.db::opt_mutations`:

```sql
CREATE TABLE opt_mutations (
  id           INTEGER PRIMARY KEY AUTOINCREMENT,
  ts           INTEGER NOT NULL,     -- unix seconds
  kernel       TEXT    NOT NULL,
  mutation     TEXT    NOT NULL,     -- human description
  brier_before REAL    NOT NULL,
  brier_after  REAL    NOT NULL,
  delta        REAL    NOT NULL,     -- negative = improvement
  accepted     INTEGER NOT NULL,
  code_diff    TEXT                   -- opaque blob (future: LLM patches)
);
CREATE INDEX idx_kernel_delta ON opt_mutations(kernel, accepted, delta);
```

The index on `(kernel, accepted, delta)` makes `memory-top` an O(log
N) query regardless of history depth.  The schema is deliberately
LLM-mutator-ready: the `code_diff` column is opaque so a future
mutator backend that emits actual C patches can store them without
schema migration.

## Pluggable mutators

The default mutator is a built-in Gaussian (1+1)-ES on one weight per
step.  To plug in an LLM-based mutator:

```
export COS_EVOLVE_MUTATOR=/path/to/mutator.sh
```

Contract (stable across releases): the mutator script receives
`params.json` on stdin + the top-N accepted mutations from `memory.db`
in a short prompt, and must emit a mutated `params.json` on stdout.
`cos-evolve` validates the output (weights normalise, τ ∈ (0,1))
before scoring.  A reference llama-server adapter ships as a future
deliverable; the present release is evaluator-only, which keeps the
self-test hermetic.

## Safety rails

- `make check-sigma-pipeline` before + after — any daemon run that
  leaves the harness red rolls back automatically (callers wire this
  via a shell wrapper; a future OMEGA-2 iteration folds the check
  into the daemon loop directly).
- Single mutation per step (1+1)-ES.  No parallel mutation swarms —
  chaos is not a strategy.
- Git integration is opt-in.  No branch/commit is created unless the
  caller passes `--git-commit` in a future release.
- τ is clamped to `(0.01, 0.99)`; degenerate weights fall back to
  uniform `0.25` each.

## Evidence

```bash
make cos-evolve
make check-cos-evolve          # full end-to-end harness
./cos-evolve self-test         # primitives only
./cos-evolve demo              # deterministic showcase
```

`check-cos-evolve` is also folded into `make check-sigma-pipeline`,
so the Ω scaffold participates in every merge-gate run.

## What this is not

- **Not AGI.**  It is a (1+1)-ES over five floats.  The *architectural*
  point — σ-gate as fitness, with optimisation memory, self-
  calibration, hypothesis harness, and a watchdog — is the story.
- **Not an overnight miracle machine.**  The daemon exists; whether it
  produces measurable gains on a real kernel is an open question that
  requires real campaign runs archived under
  [`REPRO_BUNDLE_TEMPLATE.md`](REPRO_BUNDLE_TEMPLATE.md) before any
  number is quoted in the README.
- **Not a replacement for the formal stack.**  Lean proofs in
  `hw/formal/v259/` stay unchanged.  Evolution only touches runtime
  parameters in a fixture-bounded regime where Brier convexity
  gives us a provable attractor.

## What it *is*

The first σ-gated self-improvement scaffold that is:

1. Deterministic and testable at the primitive level.
2. Fitness-aligned with the σ-gate itself (not with a proxy).
3. Safety-constrained by a watchdog that halts on σ rise.
4. Schema-stable so LLM-mutator backends can be bolted on without
   schema migrations or API churn.

That's the prerequisite for everything else labelled "OMEGA".

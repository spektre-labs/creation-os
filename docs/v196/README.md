# v196 σ-Habit — compiled routines + cortex/cerebellum σ-switch

## Problem

Human brains habituate repetitive tasks into fast, automatic
routines (cerebellum) while keeping deliberate reasoning
(cortex) for novel inputs. Agents that never compile their
routines re-run full reasoning on every call — slow and
redundant.

## σ-innovation

The v181 audit log is scanned for recurring prompt patterns;
a pattern becomes a **habit candidate** when `occurrences ≥
τ_repeat` **and** `σ_steady < τ_break`. Candidates compile
(v137 LLVM path — v0 = closed-form cycle model) to an
answer in `cycles_compiled` cycles instead of
`cycles_reasoning`.

σ is the cortex/cerebellum **switch**:

| σ on output | mode | executes |
|-------------|------|----------|
| σ < τ_break | CEREBELLUM | compiled habit (≥ 10× speedup) |
| σ ≥ τ_break | CORTEX     | full reasoning, no silent miscompile |

The 32-tick trace injects a σ spike on a compiled habit —
control returns to cortex and `cycles_reasoning` is actually
paid. Habit library is FNV-1a hash-chained.

## Merge-gate

`make check-v196` runs
`benchmarks/v196/check_v196_habit_compile_speedup.sh`
and verifies:

- self-test PASSES
- `n_habits ≥ 3`
- every habit: `speedup ≥ 10×` **and** `σ_steady < τ_break`
- non-habits: never miscompiled (fail at least one gate)
- ≥ 1 break-out tick (σ spike → CORTEX mode, cycles_reasoning used)
- `sum(cerebellum cycles) ≤ (1/10) · equivalent cortex cycles`
- chain valid + byte-deterministic

## v196.0 (shipped) vs v196.1 (named)

| | v196.0 | v196.1 |
|-|-|-|
| habit library | in-memory | `~/.creation-os/habits/*.habit` files |
| compilation | closed-form cycle model | real v137 LLVM path |
| audit scan  | embedded fixture | streaming v181 scan |
| break-out   | 1 injected σ spike | online σ monitor on every compiled emit |

## Honest claims

- **Is:** a deterministic 8-pattern / 32-tick demonstrator that
  proves ≥ 3 patterns qualify as habits, every habit runs in
  ≥ 10× fewer cycles than full reasoning, and a σ spike
  reliably breaks out to cortex without silent miscompile.
- **Is not:** a live compiled-habit cache on disk or a v137
  compile pipeline — those ship in v196.1.

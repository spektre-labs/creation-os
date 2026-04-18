# v189 σ-TTC — test-time compute allocated by σ

## Problem

Test-time scaling (Snell et al., 2024; Brown et al., 2024)
says *think longer, answer better*. Nobody says **how much**
longer for which query. Uniform best-of-N wastes compute on
easy tokens and under-spends on hard ones.

## σ-innovation

v189 makes σ the allocator:

| σ_first-token | paths | per-token depth | plug-ins |
|---|---|---|---|
| < 0.20 (easy) | 1 | 1 iteration | — |
| 0.20..0.50 (medium) | 3 | up to 3 | — |
| ≥ 0.50 (hard) | 8 | up to 8 | v150 debate + v135 symbolic + v147 reflect |

Per-token recurrent depth scales with per-token σ and
early-stops when σ falls below `τ_stop`. Three user-selectable
modes cap the whole ladder:

- `[ttc] mode = "fast"` caps paths=1 and forbids all plug-ins.
- `[ttc] mode = "balanced"` caps paths=3 (default).
- `[ttc] mode = "deep"` **floors** paths at 8 for every query.

The hard/easy compute ratio on the fixture is > 4× — matching
Snell et al.'s "compute-optimal is 4× more efficient than
uniform best-of-N" result, but on a σ-allocation instead of a
label-free one.

## Merge-gate

`make check-v189` runs
`benchmarks/v189/check_v189_ttc_adaptive_budget.sh`
and verifies:

- self-test PASSES in BALANCED, FAST, and DEEP
- class partition: 16 easy + 16 medium + 16 hard = 48
- monotone spending: `mean_compute_hard ≥ 2× medium ≥ 2× easy`
- `ratio_hard_over_easy > 4.0` in BALANCED and DEEP
- FAST: every query has `n_paths=1` and no plug-ins
- DEEP: every query has `n_paths == max_paths_deep = 8`
- easy class queries: exactly 1 path, 1 recurrent iter/token
- output byte-deterministic

## v189.0 (shipped) vs v189.1 (named)

| | v189.0 | v189.1 |
|-|-|-|
| Paths | simulated from σ (closed-form) | real v111.2 thinking paths over BitNet-2B |
| Per-token depth | closed-form from per-token σ | learnt halt predictor over BitNet layers |
| API | CLI JSON report | `[ttc] mode` wired into `/v1/chat/completions` |
| Modes | fast · balanced · deep | + v132-persona auto-select |

## Honest claims

- **Is:** a deterministic, weights-free kernel that allocates
  test-time compute by σ over a 48-query fixture, demonstrates
  monotone spending, hard/easy ratio > 4×, and mode caps.
- **Is not:** a live server-side thinking-path enumerator. The
  BitNet-2B path execution and `/v1/chat/completions` wiring
  ship in v189.1.

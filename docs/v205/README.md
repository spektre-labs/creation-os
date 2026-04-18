# v205 σ-Experiment — design + simulation-first + power + repro

## Problem

A hypothesis without a test is an opinion. A test without
a power analysis is a ritual. A test without a reproduction
trail is folklore. v205 bundles all three into one
receipted pipeline.

## σ-innovation

For each of the top-3 hypotheses from v204 v205 builds an
experiment descriptor:

| field                | meaning |
|----------------------|---------|
| `dep_var`            | dependent variable id |
| `indep_var`          | independent variable id |
| `control_var`        | control variable id — must be distinct from the above |
| `effect_size`, α, β  | inputs to the power formula |
| `n_required`         | `⌈(z_α + z_β)² / effect²⌉` (deterministic) |
| σ_design             | structural validity of the plan |
| σ_sim                | reliability of the cheap v176 simulation |
| σ_power              | budget headroom ∈ [0,1] |

**Simulation-first**: evaluate σ_power first; if the
experiment is under-powered it is marked
**UNDER_POWERED** and never runs. Otherwise the sim
result drives **SIM_SUPPORTS** vs **SIM_REFUTES**. Only
SIM_SUPPORTS graduates to the real run — the loop
explicitly saves resources on experiments that cannot
answer the question.

Reproducibility receipt: every experiment record is
hash-chained (FNV-1a). `cos reproduce --experiment N`
replays the chain byte-identically.

## Merge-gate

`make check-v205` runs
`benchmarks/v205/check_v205_experiment_design_valid.sh`
and verifies:

- self-test PASSES
- 3 experiments (one per v204 TEST_QUEUE slot)
- (dep, indep, control) are distinct variable ids
- `n_required` matches `(z_α+z_β)² / effect²` in Python
- ≥ 1 SIM_SUPPORTS + ≥ 1 UNDER_POWERED in the fixture
- σ ∈ [0,1]
- chain valid + byte-deterministic

## v205.0 (shipped) vs v205.1 (named)

|                     | v205.0 (shipped) | v205.1 (named) |
|---------------------|------------------|----------------|
| planner             | closed-form fixture | v121 planner |
| simulator           | σ_sim scalar      | v176 simulator |
| audit               | per-call hash chain | v181 streaming audit |
| n_required          | two-sided z, α=0.05, β=0.20 | domain-specific power models |

## Honest claims

- **Is:** a deterministic experiment designer that
  enforces structural validity, computes n_required
  from a canonical power formula, gates on
  simulation-first, and emits a reproducible receipt.
- **Is not:** a live planner, simulator, or power
  solver — those ship in v205.1.

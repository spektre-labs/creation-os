# v163 — σ-Evolve-Architecture · CMA-ES Over Kernel Genomes

**Header:** [`src/v163/evolve.h`](../../src/v163/evolve.h) · **Source:** [`src/v163/evolve.c`](../../src/v163/evolve.c) · **CLI:** [`src/v163/main.c`](../../src/v163/main.c) · **Gate:** [`benchmarks/v163/check_v163_evolve_pareto_converge.sh`](../../benchmarks/v163/check_v163_evolve_pareto_converge.sh) · **Make:** `check-v163`

## Problem

v136 σ-tune optimizes the parameters of the σ-aggregator. v162
σ-compose lets a user pick a profile by hand. Neither answers
the question: **given a device budget, which kernels should be
on?** That question is a search problem over a high-dimensional
genome (which kernels, in what order, with what configuration)
evaluated against a multi-objective fitness (accuracy, latency,
memory). It cannot be solved by hand.

v163 evolves the architecture itself. A CMA-ES-lite loop samples
populations of architectures, evaluates their fitness, and
updates per-gene Bernoulli means toward the elite quarter of
each generation. The result is a Pareto front over (accuracy ↑,
latency ↓, memory ↓) from which v162 picks an auto-profile for
a declared device budget.

## σ-innovation

**K(K)=K.** A system that evolves itself must measure the
quality of its evolution. v163 enforces σ-gated evolution:

```
regression = baseline_accuracy − candidate_accuracy
if regression > τ_regression (= 0.03):
    candidate.sigma_gated = true   # rejected by v133-meta
```

The baseline is the all-genes-on architecture (σ = 0.908 on the
shipped fitness). Any candidate that regresses more than 3 % is
rejected, preventing evolution from collapsing into a tiny
fast genome that loses accuracy.

## Gene table

| Gene | Kernel | Contribution | Latency | Memory |
|---|---|---:|---:|---:|
| v101 | σ-channels | 0.20 | 1 ms | 50 MB |
| v106 | HTTP server | 0.12 | 5 ms | 40 MB |
| v111 | σ-gate | 0.18 | 1 ms | 5 MB |
| v115 | memory | 0.10 | 3 ms | 80 MB |
| v117 | paged-KV | 0.04 | 1 ms | 30 MB |
| v118 | vision | 0.05 | 15 ms | 200 MB |
| v119 | planner | 0.06 | 8 ms | 25 MB |
| v124 | continual-lora | 0.03 | 12 ms | 40 MB |
| v128 | swarm | 0.02 | 6 ms | 30 MB |
| v140 | meta | 0.04 | 3 ms | 15 MB |
| v150 | debate | 0.06 | 50 ms | 60 MB |
| v152 | corpus | 0.05 | 20 ms | 100 MB |

```
accuracy  = 0.30 + Σ gene_on × contribution × (1 − 0.03 × n_on)
latency   = Σ gene_on × latency_cost_ms
memory    = Σ gene_on × memory_cost_mb
```

The saturation term captures the "more kernels ≠ always more
accuracy" phenomenon; it is what makes the Pareto front
non-trivial.

## CMA-ES-lite loop

* Population: 12
* Generations: 30
* Elite fraction: 1/4
* Gene-mean update: `p ← (1 − 0.4) p + 0.4 · freq(elite_on_gene)`
* Bernoulli clamp: `p ∈ [0.02, 0.98]` (no lock-in)

Every non-gated individual is fed into a global Pareto front
(non-dominated sort). The front is bounded at 12 points in v0.

## Auto-profile picker

Given a budget `(lat_ms, mem_mb)`, v163 picks the Pareto point
whose latency and memory both fit the budget and whose accuracy
is maximal. Falls back to the smallest-memory point if the
budget admits nothing.

Example (seed 163):

| Budget | Picked genome | Accuracy | Latency | Memory |
|---|---|---:|---:|---:|
| `(50, 200)` | — | — | — | — |
| `(500, 2000)` | 10-gene superset | ~0.93 | ~110 ms | ~600 MB |

Small budgets may leave the front empty, which is the correct
signal: *no Pareto point fits your device, upgrade or loosen*.

## Merge-gate

`make check-v163` asserts:

1. 6-contract self-test (E1..E6) passes.
2. Pareto front has ≥ 3 non-dominated points.
3. Non-dominance holds pairwise (no front point dominates
   another).
4. σ-gate holds: no front point regresses more than
   `τ_regression = 0.03` from baseline.
5. Auto-profile under a generous budget picks ≥ as many genes
   as under a tight budget (monotone in budget).
6. Byte-identical JSON under the same seed; different seeds
   produce different fronts.

## v163.0 vs v163.1

* **v163.0** — closed-form deterministic fitness. No real
  benchmarking. No real profile mutation.
* **v163.1** —
  - per-candidate real v143 benchmark smoke (50-question subset)
  - `kernels/evolve/pareto.json` persistence
  - v162 integration: `cos profile --auto --lat 50 --mem 4000`
    wires the auto-profile's enabled-gene list straight into the
    composer.

## Honest claims

* The accuracy number is a **proxy**, not a real v143 score.
  v163.0 gates convergence and Pareto structure; it does not
  claim to measure the trained model's behaviour.
* The search is deterministic per seed; "better fitness" across
  seeds is a statement about the gene table, not the real model.
* `K(K)=K` is enforced in the gate via τ_regression; the
  v133-meta supervisor that rejects candidates during training
  is a v163.1 integration.

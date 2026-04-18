# v214 σ-Swarm-Evolve — evolutionary swarm intelligence

## Problem

v150 swarm debate is static: a fixed set of agents
argue. Static swarms do not *improve* — the collective
IQ is bounded by the weakest member. v214 makes the
swarm evolve: agents are born, compete, speciate, and
are retired by a single, non-subjective fitness
function.

## σ-innovation

```
fitness = 1 / (σ_mean + ε)
```

σ itself is the fitness function. No human grader, no
RLHF proxy, no opinion polling — the agent with the
lowest mean σ across its niche benchmark is the
fittest. v136 CMA-ES plays the role of outer-loop
optimiser in v214.1; in v214.0 we use a deterministic
in-niche breeding rule that makes the same geometry
auditable on a laptop.

### Per-niche lifecycle

4 ecological niches (MATH / CODE / PROSE / BENCH);
each generation, per niche:

1. Retire the niche-local **worst** (highest σ̄).
2. Breed the niche-local top-2 → child with
   σ̄_child = max(0.05, 0.97 · mean(parents)).
3. Population is conserved *inside every niche*.

We tried the classic "retire global bottom-2" rule
first. Measured on the v0 fixture: the fleet-wide
σ̄ drops faster but diversity collapses to one niche
by generation 5 — a math specialist really does end
up competing with a code specialist, and the code
specialist loses. Switching to per-niche carrying
capacity preserves ≥ 3 established niches at
generation 10 while still driving σ_emergent down
monotonically.

### Emergence tracking

```
σ_emergent(g) = fleet-wide σ̄ at generation g
```

v192 in spirit: the self-test enforces
σ_emergent(g+1) ≤ σ_emergent(g) + 1e-6 — the swarm
gets *better*, not worse, across generations. If a
future fixture breaks that, the kernel refuses to pass
merge-gate.

## Merge-gate contract

`make check-v214-swarm-evolve-speciation` asserts:

* self-test PASSES
* 10 generations; population constant at 12 every gen
* `final_established_species ≥ 3`
* `max_lineage_span ≥ 5` (at least one lineage lives
  half the run)
* `total_retired ≥ 8` (the lifecycle actually runs)
* σ_emergent(g) is monotone non-increasing
* FNV-1a chain valid + byte-deterministic replay

## v0 / v1 split

**v214 (this kernel)**

* Deterministic closed-form fixture (12 agents × 10 gens).
* No network, no weights, no RNG.
* Replays byte-identically across platforms.

**v214.1 — named, not implemented**

* Live LoRA adapter merge via v125 PEFT.
* v136 CMA-ES outer loop picking breeding pairs.
* v143 real benchmark re-score per generation.
* v192 live emergence detection over real model runs.

## Honest claims

**v214 *is*:** a deterministic simulation of an
evolutionary-swarm lifecycle with σ-as-fitness and
per-niche speciation, auditable in < 10 ms, that
would collapse diversity under a naïve global-worst
rule and demonstrably does not under the v0 rule.

**v214 is *not*:** a live training loop, a LoRA
merging harness, or a claim that the numbers in the
fixture come from actual model benchmarks. v214.1 is
where those live; v214 is the geometry.

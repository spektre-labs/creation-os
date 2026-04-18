# v217 σ-Ecosystem — trophic levels, resource flow, symbiosis

## Problem

Individual agents → swarms → *ecosystems*. An
ecosystem is healthy when no one trophic level
dominates the others; it is fragile when one level
starves or explodes. v217 puts that dynamic into the
stack as a whole-system health snapshot.

## σ-innovation

Four trophic levels — the Creation OS agent
population mapped onto ecology:

| idx | level        | sources                         |
|-----|--------------|---------------------------------|
| 0   | PRODUCERS    | v174 flywheel data generators   |
| 1   | CONSUMERS    | v101 inference users            |
| 2   | DECOMPOSERS  | v177 compress + v159 heal       |
| 3   | PREDATORS    | v122 red-team + v210 guardian   |

Populations 32 / 28 / 22 / 18 out of 100, so no level
exceeds τ_dom = 0.40. Compute shares (0.30 / 0.38 /
0.18 / 0.14) are allocated v200-market style.

### Symbiosis

Five named pairs covering the three classical
relationships:

* **MUTUAL**    — v120 big→small distill
* **COMPETE**   — v150 debate agent vs agent
* **COMMENSAL** — v215 stigmergy leave / find
* **MUTUAL**    — v121 planner ↔ v151 coder
* **COMPETE**   — v210 guardian ↔ v122 red-team

### Aggregate σ

```
σ_dominance = max share over trophic levels
σ_balance   = 1 − min share / max share
σ_symbiosis = mean σ_pair
σ_ecosystem = 0.4·σ_dominance
             + 0.4·σ_balance
             + 0.2·σ_symbiosis
K_eff       = 1 − σ_ecosystem
```

`K_eff > τ_healthy (0.55)` on the v0 fixture — the
snapshot is engineered to be healthy so the
merge-gate catches *regressions*, not baseline
failures.

## Merge-gate contract

`make check-v217-ecosystem-trophic-balance` asserts:

* self-test PASSES
* 4 trophic levels; populations sum to `pop_total`
* no level share > τ_dom (0.40) — no dominance
* ≥ 3 symbiotic pairs, every `σ_pair ∈ [0, 1]`
* `σ_ecosystem`, `K_eff ∈ [0, 1]`
* `K_eff > τ_healthy`
* FNV-1a chain valid + byte-deterministic replay

## v0 / v1 split

**v217 (this kernel)**

* Closed-form snapshot; 4 levels × 5 pairs,
  deterministic.

**v217.1 — named, not implemented**

* Live v174 / v101 / v177 / v159 / v122 / v210
  head-counts sampled at runtime.
* Live v200 market allocation feeding compute shares.
* v193 coherence dashboard exposing σ_ecosystem and
  K_eff in real time.

## Honest claims

**v217 *is*:** a reference implementation of whole-
system σ-health aggregating dominance, balance, and
symbiosis into a single auditable number with an
FNV-1a hash chain.

**v217 is *not*:** a live scheduler, a resource
allocator, or a proof that real Creation OS
populations behave ecologically — v217.1 is where
those wires terminate. The math and the contract are
what v217 owns.

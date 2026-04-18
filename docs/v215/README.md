# v215 σ-Stigmergy — indirect communication via shared environment

## Problem

Ants coordinate without messages — they drop
pheromones, and those pheromones decay. v215 brings
the same indirection into Creation OS: agents do not
send each other messages, they leave marks in v115
memory, and other agents find those marks. No broker,
no pub/sub, no central coordinator — and therefore no
pull on the throughput budget from a scheduler that
would otherwise have to be correct.

## σ-innovation

Each mark is `(t, author_node, σ_product_write)`. Its
pheromone strength at time `t_final` is the closed
form

```
strength(t_final) = Σ_k max(0, 1 − σ_k)
                       · e^{−λ · (t_final − t_k)}
                       / normaliser
λ                 = 0.08 per step
```

normalised into `[0, 1]` by the maximum possible from
this many marks at σ = 0. Strong marks (low σ)
persist and attract; weak marks (high σ) decay and
disappear.

### Trail formation

A trail is considered *formed* when ≥ 3 distinct mesh
nodes reinforce it. v128 in spirit — the v0 fixture
records `nodes_mask` per trail and the self-test
enforces the reinforcer count, so a trail that only
lives inside one node does not count as a trail.

### Self-annihilating bad trails

If a trail leads to an error, the followers produce
high σ and their reinforcements dilute rather than
strengthen. Over 20 steps the false trails in the
fixture drop below τ_trail = 0.40 and die. σ is the
pheromone *and* the gate.

## Merge-gate contract

`make check-v215-stigmergy-trail-form` asserts:

* self-test PASSES
* 6 trails (4 true, 2 false); 20 time steps
* every TRUE trail ends alive (strength ≥ τ_trail)
  *and* has ≥ 3 distinct reinforcer nodes
* every FALSE trail decays below τ_trail
* `strength ∈ [0, 1]` for all trails
* FNV-1a chain valid + byte-deterministic replay

## v0 / v1 split

**v215 (this kernel)**

* 6 hand-picked trails × 20 steps, closed-form decay.
* Deterministic; no I/O, no memory backing.

**v215.1 — named, not implemented**

* Live v115 memory backing (marks persisted on disk).
* v128 mesh reads (marks cross node boundaries).
* v192 emergent-trail detection (unsupervised trail
  discovery rather than fixture-provided trails).

## Honest claims

**v215 *is*:** a reference implementation of σ-gated
pheromone stigmergy with closed-form decay, proving
the math and the contract shape.

**v215 is *not*:** a distributed filesystem, a mesh
implementation, or a claim that live v115 memory
behaves identically to the fixture. The trail
dynamics are the contract; the backing store is
v215.1's problem.

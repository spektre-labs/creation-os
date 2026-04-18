# v150 — σ-Swarm-Intelligence · Debate + Adversarial Verify

**Kernel:** [`src/v150/swarm.h`](../../src/v150/swarm.h), [`src/v150/swarm.c`](../../src/v150/swarm.c) · **CLI:** [`src/v150/main.c`](../../src/v150/main.c) · **Gate:** [`benchmarks/v150/check_v150_swarm_debate_converge.sh`](../../benchmarks/v150/check_v150_swarm_debate_converge.sh) · **Make:** `check-v150`

## Problem

v114 routes each task to one specialist and ships the one-shot
answer. That is strictly worse than what a committee can do:

* No cross-specialist correction.
* No σ_collective — every answer lives at one σ.
* No emergent specialisation from usage.

v150 turns one-shot routing into a **three-round debate** with
adversarial verification and a proconductor-style σ_collective
metric — the "resonance not consensus" principle made
measurable.

## σ-innovation

Three specialists (default: `atlas/math`, `crow/code`,
`lynx/bio`) each carry a unit-vector affinity on a 4-D topic
axis. Per-answer σ is derived from

```
σ = (1 − cos(affinity, topic_tag)) · 0.7  +  seeded_jitter · 0.3
```

so a matched specialist is structurally less uncertain than an
off-topic one.

Three rounds:

* **R0** — independent answers + σ.
* **R1** — adopt neighbour if neighbour's σ < own (adopter
  carries source σ + 0.01 epistemic penalty — the originator
  keeps argmin).
* **R2** — vote: every specialist emits the argmin-σ answer
  from R1, with per-specialist σ scaled by 1 / √N v104
  shrinkage.

Adversarial verification (`cos_v150_adversarial_verify`):

* every non-defender issues a σ-scored critique of the argmin
  specialist,
* `σ_critique = adversary_R1_σ + seeded_jitter`,
* `rebuttal_found` iff `σ_critique ≤ τ_rebuttal` (default 0.30),
* defender σ **rises** on rebuttal, **drops** on vindication.

Two ensemble metrics:

| Metric | Formula | Interpretation |
|---|---|---|
| **σ_consensus** | stdev(σ_final) / mean(σ_final) | low ⇒ the swarm converged |
| **σ_collective** | geomean(σ_final) | one-number proconductor metric; survives one noisy specialist |

Emergent specialisation: a per-(specialist × topic-axis) win
tally increments every time that pair is argmin; v150.1 feeds
this into v124 continual for orbital specialisation.

## Merge-gate

`make check-v150` runs:

1. Self-test (six internal contracts: debate routing, argmin
   invariant, σ_collective ≤ worst R0 σ, adversarial critique
   count, topic routing, determinism).
2. `--debate math --question "solve 2x+3=7"` → `converged:true`,
   `best:0` (atlas).
3. `σ_collective ∈ (0, 0.60)`.
4. `--verify` emits exactly N − 1 = 2 critiques.
5. `--route code` → specialist 1 (crow); `--route bio` →
   specialist 2 (lynx).
6. Same seed ⇒ byte-identical JSON.

## v150.0 vs v150.1

* **v150.0** — pure C11, deterministic SplitMix64 jitter, fixed
  3-specialist committee, no network, no weights. Topic hashing
  uses a small canonical key table + FNV fallback.
* **v150.1** —
  * v114 swarm-router supplies real specialists and answers.
  * v128 mesh gossip carries debates across nodes.
  * v124 continual consumes the win-tally and applies σ-pressure
    specialisation to each specialist's weights.
  * `/v1/debate` on v106 HTTP exposes the three-round protocol
    to external callers.

## Honest claims

* **v150.0 does not generate answers.** The "answer" strings
  are deterministic stubs that carry the specialist's name +
  topic tag + question. σ is the honest part; text is a
  placeholder until v150.1 plumbs a real LM per specialist.
* **"Consensus" is measured, not forced.** σ_consensus is a
  ratio (stdev / mean) — if the three specialists disagree,
  the metric reports it rather than hiding it. The contract is
  that we never *manufacture* agreement; we measure its
  absence.
* **σ_collective is a geomean, not an average.** That is
  deliberate: one bad specialist (σ near 1) poisons an
  arithmetic mean but only mildly perturbs a geomean, which is
  what v104 aggregation theory says we should use.

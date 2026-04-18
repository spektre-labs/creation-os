# v220 — σ-Simulate (`docs/v220/`)

A domain-agnostic Monte Carlo engine with σ-per-rule confidence and what-if
attribution.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

v149 is MuJoCo-specific; v176 generates worlds.  Neither is a **general**
rule-driven simulator.  v220 takes a typed rule set + an initial state and
emits a distribution of terminal states with σ per transition — the *same*
engine runs physics, economics, ecology, or a game.

## σ-innovation

We model a 4-state system under two rule sets:

- `baseline` — nominal 4 transition rules, each with `σ_rule ≤ τ_rule = 0.10`
- `whatif`   — one rule perturbed (here: `s2 → s3` prob drops 0.60 → 0.20)

We run 200 Monte Carlo rollouts × 8 steps per scenario.  Baseline and whatif
**share the per-rollout seed**, so the histogram delta is pure rule
attribution — Monte Carlo variance is reduced against itself.

- `σ_rule`         per-rule confidence, `∈ [0, τ_rule]`
- `σ_sim`          normalised Shannon entropy over the terminal histogram,
                   `H(hist) / log(N_STATES)`, `∈ [0, 1]`.
- `σ_engine`       `max σ_rule` across the active rule set — the simulation
                   is trusted only when `σ_engine ≤ τ_rule`.
- `σ_causal[i]`    `|p_baseline[i] − p_whatif[i]|` per terminal state —
                   v140-style attribution.  `Σ σ_causal ≤ 2` by the bound
                   on two probability distributions.

## Merge-gate contract

`bash benchmarks/v220/check_v220_simulate_monte_carlo.sh`

- self-test PASSES
- 4-state, 4-rule, 200-rollout × 8-step × 2-scenario configuration
- every `σ_rule ≤ τ_rule`; `σ_engine ≤ τ_rule`
- histograms sum to `n_mc`
- `σ_sim_baseline`, `σ_sim_whatif ∈ [0, 1]`
- histograms **differ** (perturbation really changes outcomes)
- `σ_causal ∈ [0, 1]` per state; `Σ σ_causal ≤ 2`
- chain valid + byte-deterministic

The RNG is a portable linear-congruential generator
(`*= 1664525u`, `+= 1013904223u`) so replay is identical on any platform.

## v0 vs v1 split

- **v0 (this tree)** — hard-coded 4-state transition system, closed-form
  normalised-entropy proxy, attribution by direct histogram diff, no live
  rule parser.
- **v220.1 (named, not implemented)** — live v135 Prolog rule parser,
  v169 ontology for domain-specific typing, v140 per-rule causal attribution
  with counterfactual interventions, v207-design inner-loop hook so a design
  iteration can call `simulate → evaluate → redesign`.

## Honest claims

- **Is:** a general-purpose rule-driven Monte Carlo engine with per-rule σ,
  a shared-seed what-if capability, and an explicit trust gate on the rule
  set.
- **Is not:** a physics engine, a macroeconomic model, or a belief system.
  σ_sim says how **predictable** the system is.  It does **not** say whether
  the rules are true.

---

*Spektre Labs · Creation OS · 2026*

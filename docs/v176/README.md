# v176 σ-Simulator — generated worlds + dream training

## Problem

v149 uses a single MuJoCo scene.  Real embodied learning needs
*many* scenes, each tuned to a specific weakness, plus a way
to learn without running costly physics at every step.

## σ-innovation

Four σ-gated stages:

1. **World generation.** 6 procedurally parametrised worlds
   (room size, object count, friction, mass) with
   `σ_realism` derived from deviation from a canonical room
   and `σ_difficulty` derived from object count, friction
   and room size.
2. **Curriculum.** Pick 5 realistic worlds (`σ_realism ≤
   τ_realism = 0.35`) ordered by `σ_difficulty` ascending —
   easy → hard, matching a v141-style weakness-first
   schedule.
3. **Sim-to-sim transfer.** Measure `σ_transfer = |σ_test −
   σ_train|` for every consecutive curriculum pair; flag
   `overfit = σ_transfer > τ_transfer (0.15)` when the
   jump between worlds is too wide for learning to survive.
4. **Dream training.** Run 1000 latent rollouts (Box-Muller
   sampled `σ ≈ 0.12 ± 0.05`) and assert
   `σ_dream_mean ≤ σ_real + dream_slack`: dreams are cheaper
   *and* calibrated when σ-gated.

Under the baked seed the curriculum is `[sandbox_simple,
kitchen_small, kitchen_canonical, zero_gravity_lab,
warehouse]`, two tough jumps are flagged overfit (pairs
`1→3` and `3→2`), and dream σ_mean = 0.121 beats the baseline
σ_real = 0.17.

## Merge-gate

`make check-v176` runs
`benchmarks/v176/check_v176_simulator_dream_train.sh` and
verifies:

- self-test
- 6 worlds, `kitchen_canonical` has the lowest σ_realism
- curriculum: 5 distinct ids, σ_difficulty ascending
- 4 transfer pairs; ≥ 1 `overfit` flag; σ_transfer ∈ [0,1]
- dreams: 1000 rollouts, `σ_dream_mean ≤ σ_real + slack`,
  `σ_dream_var > 0` (not collapsed to a delta)
- JSON byte-identical across runs

## v176.0 (shipped) vs v176.1 (named)

| | v176.0 | v176.1 |
|-|-|-|
| World emission | parametric fixture | real MuJoCo XML |
| Curriculum driver | σ ordering | v141 hook |
| Transfer measurement | closed-form σ-gap | real headless simulate |
| Dreams | Box-Muller σ draw | DreamerV3-style latent model |

## Honest claims

- **Is:** an offline kernel demonstrating world generation,
  curriculum ordering, transfer measurement, and dream-
  training σ-gating — all deterministic.
- **Is not:** a physics simulator.  No MuJoCo step runs in
  v0; the contract is the σ-relationships and the
  curriculum invariants.

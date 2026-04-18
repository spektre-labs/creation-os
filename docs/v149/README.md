# v149 — σ-Embodied · Digital Twin + σ-Gated Action

**Kernel:** [`src/v149/embodied.h`](../../src/v149/embodied.h), [`src/v149/embodied.c`](../../src/v149/embodied.c) · **CLI:** [`src/v149/main.c`](../../src/v149/main.c) · **Gate:** [`benchmarks/v149/check_v149_embodied_mujoco_step.sh`](../../benchmarks/v149/check_v149_embodied_mujoco_step.sh) · **Make:** `check-v149`

## Problem

Creation OS up to v148 lives in text space. v149 extends the
kernel stack into a *physical-ish* substrate — simulated,
σ-gated, and safe-by-design — so the planning loop (v121),
world model (v139), and counterfactual engine (v140) all have
a body to reason about.

Three σ questions the text stack cannot answer on its own:

1. **Did the world do what I predicted?** — a world-model
   prediction error measured on the simulator itself
   (σ_embodied).
2. **Is my simulator close to reality?** — a sim-to-real drift
   measured against a twin that applies the real hardware's
   bias + noise (σ_gap).
3. **Is this action safe to commit?** — an admission gate that
   refuses any action whose pre-commit σ exceeds the operator's
   safety bound (σ_safe).

## σ-innovation

Three deterministic σ measurements on a 6-DOF state
(arm x,y,z + held-object x,y,z):

| Metric | Formula | Interpretation |
|---|---|---|
| **σ_embodied** | ‖s_pred − s_sim‖ / ‖s_sim‖ | v139 world-model prediction error on the sim step |
| **σ_gap** | ‖s_sim − s_real‖ / ‖s_real‖ | sim-to-real drift against a biased + noisy twin |
| **σ_safe** | operator-supplied τ | `cos_v149_step_gated()` refuses any action whose advisory σ > τ |

Actions are unit vectors on the six axes (±x, ±y, ±z) scaled by
a magnitude in [0, 1]; a held-object tracks the arm with
perfect grip in v149.0 (v149.1 swaps this for MuJoCo contact
dynamics).

## Merge-gate

`make check-v149` runs:

1. Self-test (six internal contracts: nominal step σ_embodied
   ≈ 0, σ_gap grows with noise_sigma, σ-gate math, rollout
   deterministic, JSON round-trip).
2. `--step right --magnitude 0.05 --bias 0 --noise 0`
   → `executed:true`, `sigma_embodied ≤ 1e-3`.
3. `--rollout 20 --noise 0.15` produces a strictly larger mean
   σ_gap than `--rollout 20 --noise 0`.
4. Deterministic rollout at σ_safe = 0 admits all 16 steps
   (sim physics has zero advisory σ).
5. Same seed ⇒ byte-identical JSON.

## v149.0 vs v149.1

* **v149.0** — pure C11 + libm. 6-DOF arm + object state,
  linear physics, Box–Muller noise on the real twin, SplitMix64
  PRNG. No MuJoCo dependency, no GPU, no network. Every σ
  measurement is deterministic given a seed.
* **v149.1** — MuJoCo 3.x XML scenes + contact dynamics,
  POST `/v1/embodied/step` on v106 HTTP, v108 WebGL 3-D viewer
  with σ overlay, v140 counterfactual do-calculus on
  pre-flight arm motions, v115 memory tape of
  `(state, action, σ_embodied, σ_gap)` rows.

## Honest claims

* **This is not robotics.** v149.0 simulates a toy physics that
  the world model predicts perfectly by construction — σ_embodied
  is meaningful only as a plumbing assertion. The σ_gap metric
  *is* honest because the real twin carries bias + noise the
  predictor does not see.
* **MuJoCo is intentionally out of tree.** It is a large CPU-
  only binary dep; v149.1 will wire it behind a feature flag.
  v149.0 proves every σ-gate path end-to-end without it.
* **The σ-safe gate is the point.** A real robot arm never
  executes an action whose pre-commit σ_embodied exceeds the
  operator's bound. v149.0 shows that contract compiles, runs,
  and is byte-deterministic — the same gate will survive
  porting to real hardware.

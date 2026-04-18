# v228 — σ-Unified (`docs/v228/`)

Field equation, conservation law, phase transitions — the whole σ stack
written as a single scalar dynamics.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

v6..v227 are individual kernels.  v228 collects them into one scalar
field equation and exposes the three invariants they all share:
dynamics, conservation, and phase transitions.

## σ-innovation

σ(t) is a scalar σ-field aggregated over the whole system.
`K_eff(t) := 1 − σ(t)` is the effective coherence, and `τ(t)` is the
characteristic decision timescale.

### Field equation (Euler-forward, 100 steps)

```
dσ/dt = − α · K_eff(t) + β · noise(t) + γ · interaction(t)
```

with `α = 0.20`, `β = 0.02`, `γ = 0.01`, `dt = 1`, and σ clamped to
[0, 1] at every step.  `noise(t) = sin φ(t)` with a deterministic phase
accumulator, and `interaction(t) = cos(0.13 · t)`.  Initial condition:
`σ(0) = 0.90` (incoherent start).

### Noether-style conservation (1 = 1 symmetry)

`τ(t) := C / K_eff(t)` with `C = K_eff(0) · τ(0) = 0.10`, so

```
K_eff(t) · τ(t) = C   for every t
```

is an **identity** in v0, not an assumption.  v228.1 will relax this to
a *measured* conservation that only has to hold within ε.

### Phase transitions

- coherent   if `K_eff ≥ K_crit = 0.50`
- incoherent if `K_eff <  K_crit`

`n_transitions` = number of sign-flips of `(K_eff − K_crit)` along the
100-step trajectory.  The fixture guarantees ≥ 1 crossing because σ
decays from 0.90 past 0.50 under the learning term `−α · K_eff`.

### Unification map

| layer    | kernel   | role in the field equation              |
|----------|----------|------------------------------------------|
| local    | v29      | pointwise σ-aggregate at a single t      |
| global   | v193     | K_eff as the macroscopic coherence       |
| scale    | v225     | fractal: σ is the same field at each scale |
| info     | v227     | entropy / KL / free-energy reading of σ  |
| dynamics | **v228** | σ(t) evolution + conservation + phases   |

One field, one equation, one invariant: `1 = 1`.

## Merge-gate contract

`bash benchmarks/v228/check_v228_unified_field_conservation.sh`

- self-test PASSES
- 100 steps of trajectory (101 samples with t = 0)
- σ(t), K_eff(t) ∈ [0, 1]
- `|K_eff(t) · τ(t) − C| ≤ ε_cons = 1e-6` for every t
- `n_transitions ≥ 1`
- `σ_end < σ_start` (learning pressure wins)
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — closed-form Euler-forward dynamics, deterministic
  sinusoidal noise, conservation enforced by definition, phase counter
  on a 100-step trajectory.
- **v228.1 (named, not implemented)** — measured (non-definitional)
  conservation with a live ε-bound, coupling to v224 tensor / v225
  fractal / v227 free-energy channels, wiring into the v203
  civilization-collapse detector, and a true Lagrangian variational
  derivation of L = 1 − 2σ that recovers the same Euler equation.

## Honest claims

- **Is:** a deterministic, reproducible ODE with a conservation law
  enforced as an identity, a single phase crossing, and a byte-replayable
  hash chain.
- **Is not:** a field-theoretic proof of anything.  v228 is a
  demonstrator that the invariants can be expressed in one equation;
  v228.1 is where they have to survive against measured data.

---

*Spektre Labs · Creation OS · 2026*

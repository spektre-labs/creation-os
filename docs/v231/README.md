# v231 — σ-Immortal (`docs/v231/`)

Continuous backup, snapshot + restore, brain transplant, σ-continuity
verification.  Hardware can fail; v231 makes sure nothing is lost.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

A Creation OS instance evolves.  If its host fails, or we want to move
the brain to new hardware, the continuity of the instance must be
provable — not assumed.  v231 gives the guarantees in closed form.

## σ-innovation

### Trajectory and incremental snapshots

- 64-bit skill vector evolves through 10 deterministic steps.
- Each step flips at most `delta_budget = 8` skill bits (fixture flips
  4 per step) at fixture-defined offsets.
- Backup strategy:

```
snapshot[0] = full state (64 bits)
snapshot[i] = delta_i = state[i] XOR state[i-1]
```

- Incremental cost `64 + Σ popcount delta_i` is strictly smaller than
  the naive `64 · N_STEPS` baseline; the compression is measurable,
  not claimed.

### σ_continuity (the 0-loss contract)

Restore by replaying deltas:

```
restored[0] = snapshot[0]
restored[i] = restored[i-1] XOR snapshot[i]
```

Contract:

```
σ_continuity = popcount(restored_last XOR original_last) / 64 = 0
restored[t]  == state[t]   for every  t ∈ [0, N_STEPS]
```

### Brain transplant

```
target_identity = fresh FNV-1a over source identity
target_skills   = source_last_skills
σ_transplant    = popcount(target_skills ^ source_last_skills) / 64 = 0
target_identity != source_identity
```

Identity differs (new hardware / new trust-chain anchor) but the brain
is byte-identical — the same entity with a new body, not a reset.

## Merge-gate contract

`bash benchmarks/v231/check_v231_immortal_restore_continuity.sh`

- self-test PASSES
- 10 steps; every `delta_popcount ≤ 8`
- `incremental_bits < full_per_step_bits` (compression measurable)
- `σ_continuity == 0` AND `restored[t] == state[t]` for every t
- `σ_transplant == 0`
- `target_identity != source_identity`
- `target_skills == source_last_skills`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 64-bit trajectory, deterministic deltas,
  closed-form restore + transplant, FNV-1a chain.
- **v231.1 (named, not implemented)** — real `cos snapshot` / `cos
  restore` CLI, content-addressable delta object store, v128 mesh
  replication, cross-machine attestation, cryptographic signing of
  every snapshot so the brain transplant also moves the trust chain.

## Honest claims

- **Is:** a precise statement of what "0-loss continuity" means and a
  trajectory on which it is enforced bitwise, plus a well-defined
  brain-transplant primitive.
- **Is not:** a production backup system.  v231.1 is where this meets
  the disk and the network.

---

*Spektre Labs · Creation OS · 2026*

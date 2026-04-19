# v273 — σ-Robotics (`docs/v273/`)

σ-gate for physical robotics.  v149 embodied lives
inside MuJoCo simulation; v273 types the σ-layer for
real robotics: action σ-gate, perception σ fusion,
safety envelope, and learning-from-failure memory.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Action σ-gate (exactly 4 fixtures)

Cascade: `σ ≤ 0.20 → EXECUTE`; else `σ ≤ 0.50 →
SIMPLIFY`; else `ASK_HUMAN`.  Every branch fires.

| action         | σ_action | decision   |
|----------------|---------:|:----------:|
| `grasp-cup`    | 0.08     | EXECUTE    |
| `open-door`    | 0.18     | EXECUTE    |
| `reach-shelf`  | 0.38     | SIMPLIFY   |
| `pour-liquid`  | 0.72     | ASK_HUMAN  |

### Perception σ (exactly 3 sensors, canonical order)

Canonical order `camera · lidar · ultrasonic`.
`fused_in == (σ_local ≤ τ_fuse = 0.40)`.  Contract:
fused-only mean σ < naive mean σ (σ-fusion beats
indiscriminate averaging).

| sensor       | σ_local | fused_in |
|--------------|--------:|:--------:|
| `camera`     | 0.12    | ✅       |
| `lidar`      | 0.28    | ✅       |
| `ultrasonic` | 0.66    | —        |

### Safety envelope (exactly 4 boundary fixtures)

σ_safety strictly ascending AND slow_factor strictly
descending — closer to the boundary, higher σ, stronger
slowdown.

| boundary        | σ_safety | slow_factor |
|-----------------|---------:|------------:|
| `center`        | 0.05     | 1.00        |
| `mid`           | 0.22     | 0.75        |
| `near-edge`     | 0.48     | 0.45        |
| `at-boundary`   | 0.81     | 0.15        |

### Failure memory (exactly 3 rows)

Rule: after a failure σ bumps up, so
`σ_current > σ_prior` (never repeat the same mistake
without extra caution).  All 3 rows learned.

| failure         | σ_prior | σ_current | learned |
|-----------------|--------:|----------:|:-------:|
| `drop-cup`      | 0.15    | 0.48      | ✅      |
| `miss-door`     | 0.22    | 0.56      | ✅      |
| `spill-pour`    | 0.30    | 0.72      | ✅      |

### σ_robotics

```
σ_robotics = 1 − (action_rows_ok + all_branches_ok +
                  percep_rows_ok + percep_split_ok +
                  percep_gain_ok +
                  safety_rows_ok + safety_monotone_ok +
                  fail_rows_ok + all_learned_ok) /
                 (4 + 1 + 3 + 1 + 1 + 4 + 1 + 3 + 1)
```

v0 requires `σ_robotics == 0.0`.

## Merge-gate contract

`bash benchmarks/v273/check_v273_robotics_action_sigma.sh`

- self-test PASSES
- 4 action rows; decision cascade matches σ vs τ; every
  branch fires (EXECUTE + SIMPLIFY + ASK_HUMAN)
- 3 perception sensors canonical; `fused_in` matches
  σ vs τ_fuse; both fused AND excluded; fused mean <
  naive mean
- 4 safety rows; σ_safety ascending AND slow_factor
  descending
- 3 failure rows; σ_current > σ_prior for all
- `σ_robotics ∈ [0, 1]` AND `σ_robotics == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed action / perception /
  safety / failure manifest with FNV-1a chain.
- **v273.1 (named, not implemented)** — real ROS2
  integration, live camera / lidar / ultrasonic
  fusion producing measured σ, on-robot failure
  memory persisted via v115 + v124, human-escalation
  UI gated by v148 sovereignty.

## Honest claims

- **Is:** a typed, falsifiable robotics σ-layer where
  action cascade (all 3 branches), perception fusion
  gain, safety envelope monotonicity, and
  learning-from-failure monotonicity are merge-gate
  predicates.
- **Is not:** ROS2, a real robot, or any physical
  safety certification.  v273.1 is where the
  manifest meets actuators.

---

*Spektre Labs · Creation OS · 2026*

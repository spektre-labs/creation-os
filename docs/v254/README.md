# v254 — σ-Tutor (`docs/v254/`)

Full adaptive tutoring system.  v252 ships a single
Socratic session; v254 wraps it in a typed learner model,
σ-steered curriculum, multi-modal delivery, progress
tracking, and a privacy-first posture.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Learner model (exactly 4 skills — BKT)

| skill           | p_mastery | σ_mastery |
|-----------------|----------:|----------:|
| `linear_algebra`| 0.78      | 0.12      |
| `calculus`      | 0.62      | 0.18      |
| `probability`   | 0.54      | 0.21      |
| `discrete_math` | 0.71      | 0.15      |

v197 ToM is the upstream learner model.

### Curriculum (exactly 4 exercises)

Rule: `σ_difficulty = |difficulty − learner_level|`.  A
hard `τ_fit = 0.20` gate makes every exercise *on level*.

| skill           | difficulty | learner_level | σ_difficulty |
|-----------------|-----------:|--------------:|-------------:|
| `linear_algebra`| 0.80       | 0.78          | 0.02         |
| `calculus`      | 0.68       | 0.62          | 0.06         |
| `probability`   | 0.60       | 0.54          | 0.06         |
| `discrete_math` | 0.75       | 0.71          | 0.04         |

### Multi-modal teaching (exactly 4 modalities)

Rule: pick the modality with the **minimum** `σ_fit`.

| modality     | σ_fit | chosen |
|--------------|------:|:------:|
| `text`       | 0.31  |        |
| `code`       | 0.14  | ✅     |
| `simulation` | 0.27  |        |
| `dialog`     | 0.42  |        |

### Progress tracking (exactly 3 rows)

| skill            | pct_before | pct_after | Δ   |
|------------------|-----------:|----------:|----:|
| `linear_algebra` | 40         | 78        | +38 |
| `calculus`       | 55         | 62        | +7  |
| `probability`    | 50         | 54        | +4  |

Every `pct_after ≥ pct_before` AND ≥ 1 row has
`delta_pct > 0`.  v172 narrative is the story layer.

### Privacy (v182, 4 required flags, all true)

```
local_only · no_third_party · user_owned_data ·
export_supported
```

### σ_tutor

```
σ_tutor = 1 − (skills_ok + exercises_fit +
               modality_chosen_ok + progress_rows_ok +
               privacy_flags) /
              (4 + 4 + 1 + 3 + 4)
```

v0 requires `σ_tutor == 0.0`.

## Merge-gate contract

`bash benchmarks/v254/check_v254_tutor_adaptive_difficulty.sh`

- self-test PASSES
- 4 skills in canonical order, every `skill_ok`
- 4 exercises with `σ_difficulty = |difficulty − level|`
  AND `σ_difficulty ≤ τ_fit = 0.20`, every `fit`
- 4 modalities in canonical order, exactly 1 `chosen`
  AND it is the one with min σ_fit
- 3 progress rows, all non-negative, ≥ 1 positive
- 4 privacy flags, all enabled
- `σ_tutor ∈ [0, 1]` AND `σ_tutor == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed learner / curriculum /
  modality / progress / privacy manifest with FNV-1a chain.
- **v254.1 (named, not implemented)** — live BKT online-
  update loop, v141 curriculum generator against a real
  exercise bank, v113 sandbox hosting code drills, v220
  simulate driving concept visualisations, v172 live
  narrative with consented opt-in, `cos tutor --export`
  producing a signed portable archive.

## Honest claims

- **Is:** a typed, falsifiable adaptive-tutor manifest
  where the modality selection σ-gate and the τ_fit
  difficulty gate both have teeth.
- **Is not:** a running tutor.  v254.1 is where the
  manifest drives a live learner session.

---

*Spektre Labs · Creation OS · 2026*

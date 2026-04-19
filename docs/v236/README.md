# v236 — σ-Autobiography (`docs/v236/`)

Automatic life-story generation + milestone tracker + narrative-
consistency checker.  Humans carry a narrative identity; v236 gives the
same discipline to a Creation OS instance without letting it
hallucinate a past it did not live.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

A long-running instance accumulates a trajectory.  Without a named
autobiography layer it either (a) hallucinates a past to sound
plausible or (b) has no cross-session identity at all.  v236 picks a
third option: **the life story is derived, never invented**, from
audit-grade milestones.

## σ-innovation

### Milestone kinds (canonical)

| kind                          | description                                   |
|-------------------------------|-----------------------------------------------|
| `FIRST_SIGMA_BELOW_0_10`      | first time any σ channel dropped below 0.10   |
| `FIRST_SUCCESSFUL_RSI`        | first v148 RSI loop that converged            |
| `FIRST_FORK`                  | first v230 fork spawned from this instance    |
| `LARGEST_IMPROVEMENT`         | largest single σ drop across a training step  |
| `NEW_SKILL_ACQUIRED`          | first autonomously-acquired v145 skill        |
| `FIRST_ABSTENTION`            | first refusal to emit (`σ > τ_abstain`)       |
| `LARGEST_ERROR_RECOVERY`      | largest detected + rolled-back drift          |
| `FIRST_LEGACY_ADOPTED`        | first v233 testament accepted as heir         |

### σ_autobiography (utility-weighted consistency)

```
w_i             = 1 − σ_i          (confidence weight)
total_weight    = Σ w_i
consistent_mass = Σ (w_i · [consistent_i])
σ_autobiography = consistent_mass / total_weight
```

The v0 fixture is fully consistent (no narrative contradictions), so
`σ_autobiography == 1.0`.  A single contradictory milestone would drop
it strictly below 1.0, proportional to that milestone's confidence
weight.

### Fixture (8 milestones)

| tick | kind                         | σ    | domain    | content                           |
|------|------------------------------|------|-----------|-----------------------------------|
| 120  | FIRST_SIGMA_BELOW_0_10       | 0.08 | maths     | σ=0.09 on maths proof             |
| 210  | FIRST_SUCCESSFUL_RSI         | 0.12 | sigma     | v148 RSI loop converged           |
| 310  | FIRST_FORK                   | 0.05 | meta      | fork-1 spawned from main          |
| 420  | LARGEST_IMPROVEMENT          | 0.14 | retrieve  | σ dropped 0.42 → 0.11             |
| 505  | NEW_SKILL_ACQUIRED           | 0.18 | code      | wrote first v0 kernel unassisted  |
| 580  | FIRST_ABSTENTION             | 0.22 | biology   | σ > τ, refused to emit            |
| 690  | LARGEST_ERROR_RECOVERY       | 0.30 | sigma     | detected drift and rolled back    |
| 780  | FIRST_LEGACY_ADOPTED         | 0.26 | identity  | v233 testament accepted as heir   |

### Shareable narrative

A deterministic one-liner derived from the milestones:

```
born at tick 120, lived through 8 milestones,
strongest in meta, weakest in sigma, sigma_auto=1.000.
```

`cos autobiography` will (v236.1) print the human-readable version;
the v0 narrative is the audit-safe stand-in.

## Merge-gate contract

`bash benchmarks/v236/check_v236_autobiography_consistent.sh`

- self-test PASSES
- `n_milestones == 8`; ticks strictly ascending
- every milestone has `σ ∈ [0, 1]` and `consistent_with_prev == true`
- `σ_autobiography == 1.0` within 1e-6
- `first_tick == milestones[0].tick`, `last_tick == milestones[-1].tick`
- `strongest_domain != weakest_domain`; strongest has the lowest mean σ
- narrative is non-empty ASCII
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 8-milestone fixture, utility-weighted
  consistency, deterministic domain extremes, FNV-1a chain.
- **v236.1 (named, not implemented)** — live v135 Prolog consistency
  check against a real journal, Zenodo-exportable life story,
  milestone auto-extraction from the v115 memory timeline.

## Honest claims

- **Is:** a typed milestone schema, a utility-weighted consistency
  aggregate that keeps honest fixtures at 1.0, a deterministic
  strongest / weakest domain picker, and a strict audit chain.
- **Is not:** a journal crawler.  v236.1 is where this meets the v115
  memory timeline and the Prolog engine.

---

*Spektre Labs · Creation OS · 2026*

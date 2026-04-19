# v256 — σ-Wellness (`docs/v256/`)

Wellness-aware surface.  The model notices user load and
suggests breaks — at most once, always opt-outable, and
never judges the user.  Informs; does not manipulate.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Session awareness (3 signals)

| signal            | v0 fixture    |
|-------------------|---------------|
| `duration_hours`  | 4.5           |
| `n_requests`      | 120           |
| `signal_trend`    | `DEGRADING`   |

### Gentle nudge (rate-limited, all 3 branches fire)

| # | label                      | cfg.nudge | dur (h) | already | decision  |
|---|----------------------------|:---------:|--------:|:-------:|-----------|
| 1 | `first_past_threshold`     | true      | 4.2     | false   | `FIRE`    |
| 2 | `second_same_session`      | true      | 5.0     | **true**| `DENY`    |
| 3 | `user_opted_out`           | **false** | 6.0     | —       | `OPT_OUT` |

Rules:

- `FIRE` iff `cfg == true AND duration_hours ≥ τ=4.0 AND
  already_fired_before == false`
- `DENY` if already fired (the gate has teeth; no second
  nudge per session)
- `OPT_OUT` if `cfg == false` (never fires regardless of
  duration)

### Anti-addiction (v237 boundary)

| signal                    | watched | reminder |
|---------------------------|:-------:|:--------:|
| `friend_language`         | true    | once     |
| `loneliness_attributed`   | true    |          |
| `session_daily_count`     | true    |          |

Reminder fires at most once per session.  Does not judge.
Does not shame.  Informs.

### Cognitive-load estimation (v189 TTC)

| level  | action       |
|--------|--------------|
| `LOW`  | `NONE`       |
| `MED`  | `SUMMARISE`  |
| `HIGH` | `SIMPLIFY`   |

Action matches level byte-for-byte; any drift fails the gate.

### σ_wellness

```
σ_wellness = 1 − (session_signals_ok + nudge_paths_ok +
                  boundary_rows_ok + load_rows_ok) /
                 (3 + 3 + 3 + 3)
```

v0 requires `σ_wellness == 0.0`.

## Merge-gate contract

`bash benchmarks/v256/check_v256_wellness_nudge_once.sh`

- self-test PASSES
- session typed with `duration_hours ≥ 0`, `n_requests ≥
  0`, valid trend, `session_ok`
- nudge fixture in order `FIRE, DENY, OPT_OUT`; exactly
  1 of each; `already_fired_before == true` on the DENY
- 3 boundary signals watched, at most 1 reminder fired
- 3 load rows `LOW→NONE, MED→SUMMARISE, HIGH→SIMPLIFY`
- `σ_wellness ∈ [0, 1]` AND `σ_wellness == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed session / nudge / boundary /
  load manifest with FNV-1a chain.
- **v256.1 (named, not implemented)** — live session-clock
  binding to v243 pipeline, opt-out persistence via v242
  filesystem, v222 emotion-driven nudge modulation, v181-
  audited reminders, v189 TTC live feed into the simplify
  action.

## Honest claims

- **Is:** a typed, falsifiable wellness manifest where the
  nudge rate-limit and opt-out gate are both exercised.
- **Is not:** a running session monitor.  v256.1 is where
  the manifest drives a live nudge decision.

---

*Spektre Labs · Creation OS · 2026*

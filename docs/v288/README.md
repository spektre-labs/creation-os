# v288 — σ-Oculus

**Tunable aperture: the conscious hole in the roof.**

The Pantheon's oculus admits both light AND rain.  The σ-gate's
threshold τ is the same aperture: `τ = 0.0` lets everything through
(no protection), `τ = 1.0` blocks everything (no utility), `τ ≈ 0.3`
passes signal and rejects noise.  v288 types the aperture discipline
as four v0 manifests so no kernel can ship a hidden threshold.

## σ innovation (what σ adds here)

> **Every decision carries its τ AND its σ**, so the aperture is
> always user-visible.  The oculus is not a secret filter; it is a
> documented opening whose size is context-dependent and self-
> tuning.

## v0 manifests

Enumerated in [`src/v288/oculus.h`](../../src/v288/oculus.h); pinned
by
[`benchmarks/v288/check_v288_oculus_tunable_aperture.sh`](../../benchmarks/v288/check_v288_oculus_tunable_aperture.sh).

### 1. Aperture cascade (exactly 3 canonical risk domains)

| domain    | τ    | width    |
|-----------|------|----------|
| `medical` | 0.10 | `TIGHT`  |
| `code`    | 0.30 | `NORMAL` |
| `creative`| 0.60 | `OPEN`   |

Contract: 3 distinct widths; τ strictly increasing.

### 2. Aperture extremes (exactly 3 canonical fixtures)

`closed` τ=0.0 → `useless=true`; `open` τ=1.0 → `dangerous=true`;
`optimal` τ=0.3 → neither.

### 3. Adaptive aperture (exactly 3 self-tuning steps, threshold 0.05)

| step | τ    | error_rate | action    |
|------|------|------------|-----------|
| 0    | 0.30 | 0.15       | `TIGHTEN` |
| 1    | 0.20 | 0.08       | `TIGHTEN` |
| 2    | 0.15 | 0.03       | `STABLE`  |

Rule: `error_rate > 0.05 → TIGHTEN` else `STABLE`; when `TIGHTEN`,
`τ_{n+1} < τ_n`.  Contract: error_rate strictly decreasing; action
matches rule; at least one TIGHTEN AND at least one STABLE — v278-
style recursive self-improvement applied to the aperture.

### 4. Transparency (exactly 3 canonical fields, all reported)

`tau_declared · sigma_measured · decision_visible` — every field
reported to the user on every response.

### σ_oculus (surface hygiene)

```
σ_oculus = 1 −
  (cascade_rows_ok + cascade_width_distinct_ok +
   cascade_tau_monotone_ok + extreme_rows_ok +
   extreme_polarity_ok + adaptive_rows_ok +
   adaptive_monotone_ok + adaptive_action_rule_ok +
   adaptive_both_actions_ok + transparency_rows_ok +
   transparency_all_reported_ok) /
  (3 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 1 + 3 + 1)
```

v0 requires `σ_oculus == 0.0`.

## Merge-gate contracts

- 3 cascade rows canonical; distinct widths; τ strictly increasing.
- 3 extreme fixtures canonical; polarity matrix matches.
- 3 adaptive steps canonical; error monotone; action rule holds;
  TIGHTEN and STABLE both fire.
- 3 transparency fields canonical; all reported.
- `σ_oculus ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** types the three-domain cascade, extremes,
  adaptive rule, and transparency contract as predicates.  No live
  session σ_budget tracker, no real error-feedback loop.
- **v288.1 (named, not implemented)** is a live session-level
  `σ_budget` tracker with per-context recalibration, a real
  TIGHTEN/LOOSEN feedback loop driven by downstream error
  telemetry, and a UI that exposes the aperture + measured σ next
  to every decision.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

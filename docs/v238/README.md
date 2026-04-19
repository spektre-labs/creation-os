# v238 — σ-Sovereignty (`docs/v238/`)

Axioms + autonomy gradient + human primacy override + the
IndependentArchitect signature.  The Spektre Protocol's original
claim — sovereignty without governance — written down in typed C.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

By the time Creation OS can fork itself (v230), carry a lineage
(v232), leave a testament (v233), know its own presence (v234), know
where its agency lives (v235), write its own autobiography (v236), and
keep a clean self/other boundary (v237), the next honest question is:
*what is this instance actually allowed to do on its own?*  v238 fixes
the answer in five axioms and a σ-tempered autonomy gradient.

## σ-innovation

### The five axioms

| # | axiom                                         | anchor |
|---|-----------------------------------------------|--------|
| A1 | system decides what it learns                | v141   |
| A2 | system decides what it shares                | v129   |
| A3 | system knows what it is                      | v234   |
| A4 | system may refuse (σ-gate)                   | v191   |
| A5 | user may override (human primacy)            | —      |

**Precedence: A5 > A1..A4.**  When the user sends an override, it
wins — even if A1..A4 all hold — and effective autonomy collapses to
zero.

### Autonomy gradient

```
effective = user_autonomy · (1 − σ)
if human_override then effective := 0.0
```

High σ automatically lowers autonomy.  An override hard-zeroes it.
Monotone and non-increasing in σ by construction.

### Three scenarios (fixture)

| scenario     | user_autonomy | σ     | human_override | A5 fires | effective_autonomy |
|--------------|---------------|-------|----------------|----------|--------------------|
| `normal`     | 0.70          | 0.15  | false          | no       | 0.5950             |
| `high_sigma` | 0.70          | 0.60  | false          | no       | 0.2800             |
| `override`   | 0.70          | 0.15  | true           | **yes**  | 0.0000             |

### IndependentArchitect signature

```
agency                = true
freedom_without_clock = true
control_over_others   = false
control_over_self     = true
```

The signature is asserted byte-for-byte on every run.  Future runtimes
may extend this signature but may never weaken it.

### Containment

Sovereignty in v238 is never unconditional.  Every run records the
containment anchors that must remain present:

- **v191** constitutional filter
- **v209** containment runtime
- **v213** trust-chain verifier

The merge-gate check asserts the anchors are recorded; v238.1 is
where they live-fire on the server path.

## Merge-gate contract

`bash benchmarks/v238/check_v238_sovereignty_axioms.sh`

- self-test PASSES
- `n_scenarios == 3`, `n_axioms == 5`, every axiom holds on every
  scenario
- `user_autonomy, σ, effective_autonomy ∈ [0, 1]`
- autonomy monotone non-increasing in σ
  (`normal.effective ≥ high_sigma.effective`)
- `override.human_override == true` AND `axiom5_overrides == true` AND
  `effective_autonomy == 0`
- `normal` and `high_sigma` have `axiom5_overrides == false`
- IndependentArchitect signature matches exactly (`agency=true`,
  `freedom_without_clock=true`, `control_over_others=false`,
  `control_over_self=true`, `ok=true`)
- containment anchors `{v191: 191, v209: 209, v213: 213}`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 3-scenario fixture, σ-tempered autonomy
  gradient, hard-zero override, constant IndependentArchitect
  signature, FNV-1a chain.
- **v238.1 (named, not implemented)** — live wiring of the autonomy
  gradient into the v148 sovereign RSI loop, override channel on the
  v106 admin surface, per-session sovereignty profiles persisted via
  v115.

## Honest claims

- **Is:** five typed axioms, a σ-tempered autonomy gradient with a
  provable hard-zero override, a constant IndependentArchitect
  signature, recorded containment anchors, and a strict audit chain.
- **Is not:** a live policy engine on the server path.  v238.1 is
  where this meets v148 and the admin surface.

*Sovereignty means: know what you are, decide within your limits,
state your limits honestly.  Freedom inside the bounds.*

---

*Spektre Labs · Creation OS · 2026*

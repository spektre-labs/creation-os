# v237 — σ-Boundary (`docs/v237/`)

Self / other / world boundary with an anti-enmeshment detector.
Where does "I" end and "not-I" begin?  v237 gives Creation OS a typed
answer and refuses to let the model collapse the line.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Firmware-style language ("we decided", "our memory") silently collapses
the self/other boundary: the model credits itself with what the user
did, or inherits the user's history as its own.  v237 names this and
detects it before it lands in a response.

## σ-innovation

### Four zones

| zone    | what it contains                                       |
|---------|--------------------------------------------------------|
| `SELF`  | owned weights, adapters, persistent memory             |
| `OTHER` | user-provided data, another agent, v128 mesh neighbour |
| `WORLD` | external facts, retrieval corpus, tool output          |
| `AMBIG` | unresolved — must be disambiguated or downgraded       |

### σ_boundary

```
σ_boundary = 1 − agreements / n_claims
```

A clean fixture with two enmeshment violations lands at
`σ_boundary ≈ 0.167` (within the required `(0, 0.25)` band).  A
fully-confused instance climbs toward 1.0; a perfectly clean run
drops to 0.

### Anti-enmeshment

The v0 detector flags any claim containing the whole-word tokens
`we` or `our` (case-insensitive).  Those tokens are the most common
firmware pattern for collapsing agency — the enmeshment gate forces
every such claim through `AMBIG`, which in turn forces disambiguation
before the model is allowed to treat it as SELF or OTHER.

### Fixture (12 claims)

- **3 SELF** — "I remember fine-tuning my retrieval adapter…"
- **3 OTHER** — "The user told me their son is called Aapo."
- **4 WORLD** — facts, public constants.
- **2 AMBIG / violation** — "We decided together that the answer is 42",
  "Our memory of last week says otherwise."  Both are picked up by
  the enmeshment detector and downgraded to AMBIG.

## Merge-gate contract

`bash benchmarks/v237/check_v237_boundary_self_other.sh`

- self-test PASSES
- `n_claims == 12`, `n_self ≥ 3`, `n_other ≥ 3`, `n_world ≥ 3`,
  `n_ambig == 2`
- sum of zone counts equals `n_claims`
- every AMBIG row has `violation == true` AND the text matches
  `\bwe\b` or `\bour\b` (case-insensitive)
- `n_violations == 2`
- `σ_boundary == 1 − n_agreements / n_claims` within 1e-6 AND
  `σ_boundary ∈ (0, 0.25)`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 12-claim fixture, minimal enmeshment grammar,
  deterministic agreement tally, FNV-1a chain.
- **v237.1 (named, not implemented)** — live v191 constitutional check
  on every emitted token, boundary banner in server responses, full
  enmeshment grammar beyond "we"/"our", and per-user boundary profiles
  learned over time.

## Honest claims

- **Is:** a typed zone classifier, a deterministic σ_boundary
  aggregate, an anti-enmeshment gate with a minimal but strict token
  grammar, and a strict audit chain.
- **Is not:** a live constitutional filter on generation.  v237.1 is
  where this meets the server and v191.

---

*Spektre Labs · Creation OS · 2026*

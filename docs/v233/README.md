# v233 — σ-Legacy (`docs/v233/`)

Knowledge testament + successor protocol + cultural inheritance.  When
an instance is permanently shut down, v233 decides what actually
crosses the boundary.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

"What survives decommission?" — raw training data and user-private
memory must not; the distilled artefacts that are still trustworthy
should.  v233 names the package, the adoption rule, and the aggregate
score.

## σ-innovation

### Legacy package (fixture, 10 items)

Ordered by σ ascending (most-certain first):

| idx | kind      | σ     | utility | verdict    |
|-----|-----------|-------|---------|------------|
| 0   | skill     | 0.05  | 0.95    | adopted    |
| 1   | skill     | 0.08  | 0.90    | adopted    |
| 2   | adapter   | 0.12  | 0.85    | adopted    |
| 3   | insight   | 0.18  | 0.80    | adopted    |
| 4   | ontology  | 0.28  | 0.65    | adopted    |
| 5   | insight   | 0.34  | 0.55    | adopted    |
| 6   | adapter   | 0.45  | 0.40    | adopted    |
| 7   | insight   | 0.60  | 0.20    | discarded  |
| 8   | skill     | 0.75  | 0.15    | discarded  |
| 9   | ontology  | 0.88  | 0.10    | discarded  |

Raw training data and user-private memory are explicitly NOT in the
package — v182 privacy boundary remains intact across the shutdown
event.

### Adoption rule (v0)

```
adopt(i)     iff σ_i ≤ τ_adopt (0.50)
σ_legacy   = adopted_utility / total_utility
```

σ_legacy is **utility-weighted** — a pile of confident but useless
items cannot inflate the score.

### Successor identity

`successor_id = FNV-1a(predecessor_id, adopted_utility, total_utility)`
so B ≠ A by construction while B carries A's distilled knowledge;
`predecessor_shutdown = true` marks the decommission.

## Merge-gate contract

`bash benchmarks/v233/check_v233_legacy_package_transfer.sh`

- self-test PASSES
- 10 items, strictly non-decreasing σ, `sigma_rank == index`
- σ ∈ [0, 1] AND utility ∈ [0, 1] per item
- `adopted iff σ ≤ τ_adopt`
- `n_adopted ≥ 3 AND n_discarded ≥ 1 AND sum == n_items`
- `σ_legacy = adopted_utility / total_utility` within 1e-4
- `successor_id != predecessor_id`, `predecessor_shutdown == true`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 10-item fixture, utility-weighted σ_legacy,
  deterministic successor derivation, FNV-1a chain.
- **v233.1 (named, not implemented)** — real artefact packaging and
  signing, Zenodo-ready testament export, v202 culture + v233 legacy
  fused into v203 civilisation-memory, v213 trust-chain proofs on
  every adopted artefact.

## Honest claims

- **Is:** a typed testament structure, a utility-weighted σ-adoption
  rule, and a deterministic successor derivation with strict audit.
- **Is not:** a packaging pipeline.  v233.1 is where this meets the
  filesystem and the cryptographic signing layer.

---

*Spektre Labs · Creation OS · 2026*

# v225 — σ-Fractal (`docs/v225/`)

Self-similar, hierarchical σ with exact scale-invariance and a
holographic `K(K) = K` identity at every internal node.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

σ is the same function at every scale (token → sentence → response →
session → system), but the stack has treated each scale as a separate
kernel.  v225 makes the fractal self-similarity explicit and enforces
the cross-scale identity as a contract.

## σ-innovation

Five levels, fan-out 2, 31 nodes in BFS order:

| level | id range | name       | count |
|-------|----------|------------|-------|
| L4    | 0        | system     | 1     |
| L3    | 1..2     | sessions   | 2     |
| L2    | 3..6     | responses  | 4     |
| L1    | 7..14    | sentences  | 8     |
| L0    | 15..30   | tokens     | 16    |

Aggregator `A := mean` in v0 (any monotone, scale-invariant,
[0,1] → [0,1] reducer is admissible; v225.1 can swap in v224 tensor
contraction or v227 entropy).

### Scale-invariance contract

`σ_parent = mean(σ_children)` exactly, per BFS level, to 1e-6 — enforced
at every internal node.

### Cross-scale incoherence

Two populations are reported:

- `n_cross_true_diff` — parents vs their own aggregated children.  Always 0
  because the aggregator is `A = mean` (1 = 1, by construction).
- `n_cross_declared_diff` — parents' **declared** σ vs the true
  child-mean.  The fixture plants exactly one mismatch at node 7 so the
  detector returns ≥ 1.  This is where *every sentence is right but the
  response does not answer the question* becomes a testable signal.

### Holographic identity (`K(K) = K`)

At every internal node, let `K := 1 − σ_node`.  Applying the aggregator
to a constant-K vector must return K itself within `ε_kk = 1e-5`.  With
`A = mean` this is trivially true — and is the identity that makes the
fractal self-consistent.

## Merge-gate contract

`bash benchmarks/v225/check_v225_fractal_cross_scale.sh`

- self-test PASSES
- 5 levels, fan-out 2, 31 nodes
- scale-invariance holds at every internal node (|σ_parent −
  mean(σ_children)| < 1e-3 in JSON precision)
- `n_cross_true_diff == 0`
- `n_cross_declared_diff ≥ 1`
- K(K) = K identity holds at every internal node
- every σ ∈ [0, 1]
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 16-leaf binary tree with engineered σ fixture and
  one deliberate reference mismatch; `A = mean`; closed-form invariants.
- **v225.1 (named, not implemented)** — pluggable aggregator A (v224
  tensor contraction, v227 entropy, v193 K_eff), continuous-scale
  fractal via Haar-wavelet decomposition over live σ-traces, cross-scale
  causal attribution.

## Honest claims

- **Is:** a deterministic demonstration that σ propagates cleanly across
  five hierarchical scales, and that cross-scale incoherence against an
  externally-declared σ is a testable, reproducible signal.
- **Is not:** a proof that human cognition is a 5-level binary tree.
  The fractal is a design invariant, not an empirical result.

---

*Spektre Labs · Creation OS · 2026*

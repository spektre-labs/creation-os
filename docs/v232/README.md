# v232 — σ-Lineage (`docs/v232/`)

Family tree + ancestor query + σ-gated merge-back.  v214 swarm-evolve
produces temporal generations; v230 σ-fork produces spatial copies.
v232 is the audit layer that keeps the whole genealogy queryable.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Once forks, swarms, and generations co-exist, the family tree becomes
the primary audit structure.  "Who is the ancestor of fork-3?" and
"Can fork-3 merge back?" are both lineage questions, not fork
questions.

## σ-innovation

### Tree (fixture)

```
node 0 (gen 0)  ROOT (v229 seed)
  ├─ node 1 (gen 1)
  │    ├─ node 3 (gen 2)
  │    └─ node 4 (gen 2)
  └─ node 2 (gen 1)
       └─ node 5 (gen 2)
```

Each node carries a 64-bit skill vector derived from its parent by a
fixture XOR edge mask.  Per-node metrics:

- `n_skills    = popcount(skills)`
- `σ_profile   = n_skills / 64`
- `σ_divergence_from_parent = popcount(edge_mask) / 64`
- `uptime_steps` — fixture value; older ancestors have more uptime.

### Ancestor query

Precomputed per node: `ancestor_path[0..gen]`, walking root → ... →
node.  `ancestor_depth == gen` by contract, so
`cos lineage --instance fork-3` is just array indexing at v0.

### σ-gated merge-back

```
σ_merge(n)      = σ_divergence_from_parent(n)
mergeable(n)   iff  σ_merge ≤ τ_merge (0.40)
```

Fixture tuning:

| node | σ_merge | verdict    |
|------|---------|------------|
| 1    | 0.06    | mergeable  |
| 2    | 0.50    | blocked    |
| 3    | 0.06    | mergeable  |
| 4    | 0.47    | blocked    |
| 5    | 0.06    | mergeable  |

→ `n_mergeable = 3`, `n_blocked = 2`, and
`n_mergeable + n_blocked = n_nodes − 1` (the root has nothing to merge
into).

## Merge-gate contract

`bash benchmarks/v232/check_v232_lineage_tree_query.sh`

- self-test PASSES
- 6 nodes, `max_gen = 2`, exactly one root
- every non-root's parent has strictly smaller `gen`
- σ_divergence ∈ [0, 1]
- `ancestor_path[0]` is a root, `ancestor_path[last] == id`,
  `ancestor_depth == gen`
- `mergeable iff σ_merge ≤ τ_merge` (per non-root)
- `n_mergeable ≥ 1 AND n_blocked ≥ 1 AND n_mergeable + n_blocked ==
  n_nodes − 1`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 6-node fixture, deterministic XOR edges,
  precomputed ancestor paths, closed-form σ-gated merge verdicts,
  FNV-1a chain.
- **v232.1 (named, not implemented)** — web UI `/lineage`, live v129
  federated merge driven by real skill-vector deltas, v201 diplomacy
  conflict resolution for blocked merges, v213 trust-chain proofs on
  every ancestor edge.

## Honest claims

- **Is:** a typed family-tree audit structure with ancestor walks, a
  σ-gated merge-back rule, and byte-replayable determinism.
- **Is not:** a live genealogy across the whole instance graph.
  v232.1 is where the tree meets the network.

---

*Spektre Labs · Creation OS · 2026*

# v235 — σ-Locus (`docs/v235/`)

Locus of agency in a distributed mesh.  Where is "I" when Creation OS
runs on v128 mesh replication?  v235 names the answer: the locus is
wherever σ is lowest on this topic, *right now*.  Mesh instances are
not independent "selves" — they are aspects of one system, the way two
hemispheres share one mind.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Once v128 lets four Creation OS nodes run in parallel, the old
"single-master" answer to *who decides?* is both wrong and unsafe.
v235 fixes the alternative:

- The locus is **per-topic**, not per-mesh.
- The locus is **dynamic**: whichever node has the lowest σ speaks.
- Migration is **explicit**, never silent.
- Split-brain is **resolved** by audit-chain length (longer = true).

## σ-innovation

### Dynamic locus per topic

For each topic the locus is `argmin σ` over the four nodes.  Ties
break on the lowest `node_id`.  The fixture seeds three topics
(`maths-proof`, `biology-QA`, `code-gen`) with a pre-snapshot and a
post-snapshot — locus migrates on two of the three.

### σ_locus_unity

```
σ_locus_unity = 1 − mean( |σ_i − σ_min| )
```

High unity ⇒ the mesh agrees and the locus label is mostly cosmetic.
Low unity ⇒ the mesh is genuinely split and the declared locus matters.

### Split-brain resolution (anti-split-brain)

After a simulated network cut the mesh divides into partition A
(3 nodes, audit chain length 17) and partition B (1 node, length 11).
On reconnect v178 Byzantine consensus awards the truth to the longer
chain; the shorter partition is flagged **fork** (v230) and must merge
back rather than claim primacy.

## Fixture (3 topics × 4 nodes)

| topic         | σ_before (A,B,C,D)            | locus_before | σ_after (A,B,C,D)             | locus_after | migrated |
|---------------|-------------------------------|--------------|-------------------------------|-------------|----------|
| maths-proof   | (0.45, 0.12, 0.30, 0.80)      | B            | (0.05, 0.20, 0.28, 0.75)      | A           | yes      |
| biology-QA    | (0.60, 0.62, 0.61, 0.63)      | A            | (0.59, 0.61, 0.60, 0.62)      | A           | no       |
| code-gen      | (0.35, 0.40, 0.20, 0.50)      | C            | (0.30, 0.38, 0.25, 0.14)      | D           | yes      |

## Merge-gate contract

`bash benchmarks/v235/check_v235_locus_migration.sh`

- self-test PASSES
- `n_nodes == 4`, `n_topics == 3`
- per topic: `locus = argmin(σ)`, `σ_locus_unity ∈ [0, 1]`,
  `σ_min_after == σ_after[locus_after]`
- every σ sample in `[0, 1]`
- `n_migrations ≥ 1`
- split-brain: `winner_partition = 0 iff chain_len_a ≥ chain_len_b`
  else `1`, `loser_flagged_fork == true`
- `σ_locus_mean_unity ∈ [0, 1]`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — 4 nodes × 3 topics fixture, argmin locus,
  deterministic audit-chain winner, FNV-1a chain.
- **v235.1 (named, not implemented)** — live v128 mesh wiring, real
  audit-chain tracking through v213 trust-chain, migration banner
  ("agency moved to node-B because σ(B) < σ(A) on maths-proof") on
  every server response.

## Honest claims

- **Is:** a deterministic argmin-based locus model, a per-topic unity
  score, a split-brain resolver that prefers the longer audit chain,
  and a strict audit chain.
- **Is not:** a live distributed consensus engine.  v235.1 is where
  this meets v128 and v213 on the wire.

---

*Spektre Labs · Creation OS · 2026*

# v263 — σ-Mesh-Engram (`docs/v263/`)

Distributed O(1) hash table for Engram.  Three nodes
(A, B, C) hold contiguous, non-overlapping shards of
[0, 256); σ-consistent replication covers critical facts
on all three; a typed memory hierarchy (L1 SQLite →
L2 Engram → L3 mesh-Engram → L4 API) and a σ-driven
forgetting policy keep the mesh honest.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Mesh nodes (exactly 3)

| node | shard_lo | shard_hi |
|------|---------:|---------:|
| A    | 0        | 86       |
| B    | 86       | 171      |
| C    | 171      | 256      |

Union is exactly [0, 256); no overlap.

### Lookup fixtures (exactly 4, every node hit)

| entity                     | hash8 | expected | served_by | lookup_ns |
|----------------------------|------:|----------|-----------|----------:|
| `entity:Helsinki`          | …     | X        | X         | ≤ 100     |
| `entity:DeepSeek`          | …     | X        | X         | ≤ 100     |
| `fact:speed_of_light`      | …     | X        | X         | ≤ 100     |
| `syntax:subject_verb_obj`  | …     | X        | X         | ≤ 100     |

`hash8` is computed at run-time; the gate asserts
`served_by == expected_node` AND every node in {A, B, C}
serves at least one fixture.

### σ-consistent replication (exactly 4 rows)

Rule: `quorum_ok ⇔ σ_replication ≤ τ_quorum = 0.25`.

| fact                       | replicas | σ_replication | quorum_ok |
|----------------------------|---------:|--------------:|:---------:|
| `AGPL_v3_header`           | 3        | 0.09          | ✅        |
| `speed_of_light_m_s`       | 3        | 0.12          | ✅        |
| `EU_AI_Act_risk_class`     | 3        | 0.38          | ❌        |
| `Creation_OS_SCSL`         | 3        | 0.06          | ✅        |

Both branches fire.

### Memory hierarchy (exactly 4 tiers)

| tier | backend         | latency_ns  | capacity_mb  |
|------|-----------------|------------:|-------------:|
| L1   | `local_sqlite`  | 10          | 10           |
| L2   | `engram_dram`   | 80          | 8 192        |
| L3   | `mesh_engram`   | 2 000       | 65 536       |
| L4   | `api_search`    | 40 000 000  | unbounded    |

Latency and capacity strictly ascending L1→L4.  L4
capacity is encoded as `UINT32_MAX` to mean "unbounded".

### Forgetting with σ (exactly 4 rows, every branch)

| item                   | σ_relevance | action    |
|------------------------|------------:|-----------|
| `session_token`        | 0.10        | `KEEP_L1` |
| `last_week_fact`       | 0.35        | `MOVE_L2` |
| `old_paper_citation`   | 0.65        | `MOVE_L3` |
| `noise_2019_tweet`     | 0.92        | `DROP`    |

Rule:

- `σ ≤ 0.20` → `KEEP_L1`
- `0.20 < σ ≤ 0.50` → `MOVE_L2`
- `0.50 < σ ≤ 0.80` → `MOVE_L3`
- `σ > 0.80` → `DROP`

### σ_mesh_engram

```
σ_mesh_engram = 1 −
  (shards_ok + lookups_ok + nodes_covered_ok +
   replication_rows_ok + both_quorum_branches_ok +
   hierarchy_ok + forgetting_rows_ok +
   forgetting_branches_ok) /
  (1 + 4 + 1 + 4 + 1 + 1 + 4 + 1)
```

v0 requires `σ_mesh_engram == 0.0`.

## Merge-gate contract

`bash benchmarks/v263/check_v263_mesh_engram_distributed.sh`

- self-test PASSES
- 3 nodes, shards contiguous, non-overlapping, cover
  [0, 256)
- 4 lookups; `served_by == expected_node`, lookup_ns ≤
  100; every node ∈ {A, B, C} serves ≥ 1 fixture
- 4 replication rows with `replicas == 3`; both
  `quorum_ok == true` and `== false` branches fire
- 4 hierarchy tiers L1..L4, latency + capacity strictly
  ascending
- 4 forgetting rows, action matches σ rule; every branch
  fires ≥ 1×
- `σ_mesh_engram ∈ [0, 1]` AND `σ_mesh_engram == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed mesh / lookup / replication
  / hierarchy / forgetting manifest with FNV-1a chain.
- **v263.1 (named, not implemented)** — live gossip on
  v128 mesh, live quorum via v216, σ-driven eviction
  wired to v115 / v242 storage backends.

## Honest claims

- **Is:** a typed, falsifiable distributed-hash manifest
  where shard coverage, per-node routing, quorum
  branches, hierarchy ordering, and forgetting policy
  are all merge-gate predicates.
- **Is not:** a running mesh.  v263.1 is where the
  manifest drives live gossip and eviction.

---

*Spektre Labs · Creation OS · 2026*

# v299 — σ-Knowledge-Graph

**Grounded reasoning; σ measures how much the anchor holds.**

LLMs hallucinate because they have no anchor. A knowledge graph
is the anchor. σ measures the anchor: when σ is low, the graph
answers directly; when σ is high, the gate abstains to an LLM
fallback (and the LLM itself is σ-gated). Every triplet carries
a provenance σ, multi-hop chains compound σ so a long chain is
noisier by construction, and the "corpus as KG" manifest lists
the canonical σ vocabulary as already-queryable triplets.

## σ innovation (what σ adds here)

> **A KG-backed answer plus a σ is a verifiable answer.**
> Instead of "the model said X", v299 emits "the KG says X
> because source Y says it, with σ_provenance = 0.05" — and
> when the KG does not know, it says so.

## v0 manifests

Enumerated in [`src/v299/knowledge_graph.h`](../../src/v299/knowledge_graph.h);
pinned by
[`benchmarks/v299/check_v299_knowledge_graph_sigma.sh`](../../benchmarks/v299/check_v299_knowledge_graph_sigma.sh).

### 1. σ-grounded retrieval (exactly 3 canonical rows)

| query_kind      | σ_retrieval | route           |
|-----------------|-------------|-----------------|
| `known_fact`    | 0.08        | `FROM_KG`       |
| `partial_match` | 0.35        | `FROM_KG`       |
| `unknown`       | 0.85        | `FALLBACK_LLM`  |

3 rows in order; `σ_retrieval` strictly increasing;
`route == FROM_KG iff σ_retrieval ≤ τ_kg = 0.40` with both
branches firing; exactly 2 `FROM_KG` AND exactly 1
`FALLBACK_LLM` — the KG answers when it can, the gate abstains
when it cannot.

### 2. Provenance tracking (exactly 3 canonical rows)

| source             | σ_provenance | trusted | peer_reviewed | source_ref            |
|--------------------|--------------|---------|---------------|-----------------------|
| `primary_source`   | 0.05         | `true`  | `true`        | `arxiv:2403.12345`    |
| `secondary_source` | 0.25         | `true`  | `false`       | `blog:spektre_labs`   |
| `rumor_source`     | 0.80         | `false` | `false`       | ``                    |

`trusted iff σ_provenance ≤ τ_prov = 0.50` with both branches
firing; every trusted row carries a non-empty `source_ref` —
a KG fact is the pair (triplet, source), not just the triplet.

### 3. Multi-hop σ (exactly 3 canonical chains)

| label   | hops | σ_per_hop | σ_total | warning |
|---------|------|-----------|---------|---------|
| `1_hop` | 1    | 0.10      | 0.100   | `false` |
| `3_hop` | 3    | 0.15      | 0.386   | `false` |
| `5_hop` | 5    | 0.20      | 0.672   | `true`  |

Chains composed via `σ_total = 1 − (1 − σ_per_hop)^hops` (within
1e-3); `hops` and `σ_total` both strictly increasing;
`warning iff σ_total > τ_warning = 0.50` with both branches
firing — a longer chain is not just slower, it is noisier.

### 4. Corpus as KG (exactly 3 canonical triplets)

| subject          | relation     | object                 |
|------------------|--------------|------------------------|
| `sigma`          | `IS_SNR_OF`  | `noise_signal_ratio`   |
| `k_eff`          | `DEPENDS_ON` | `sigma`                |
| `one_equals_one` | `EXPRESSES`  | `self_consistency`     |

Every row `well_formed = true` (non-empty subject AND relation
AND object) AND `queryable = true` — the σ vocabulary is
already a graph, not free text.

### σ_kg (surface hygiene)

```
σ_kg = 1 −
  (ret_rows_ok + ret_order_ok + ret_gate_polarity_ok + ret_fromkg_count_ok +
   prov_rows_ok + prov_order_ok + prov_polarity_ok + prov_source_ok +
   hop_rows_ok + hop_order_ok + hop_formula_ok + hop_warning_polarity_ok +
   corpus_rows_ok + corpus_wellformed_ok + corpus_queryable_ok) /
  (3 + 1 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1)
```

v0 requires `σ_kg == 0.0`.

## Merge-gate contracts

- 3 retrieval rows; σ strictly increasing; `FROM_KG iff σ ≤ 0.40`
  with both branches firing; 2 `FROM_KG` AND 1 `FALLBACK_LLM`.
- 3 provenance rows; σ strictly increasing; `trusted iff σ ≤ 0.50`
  with both branches firing; trusted rows carry a source reference.
- 3 multi-hop chains; hops and σ_total strictly increasing;
  σ_total matches `1 − (1 − σ_per_hop)^hops` within 1e-3;
  `warning iff σ_total > 0.50` with both branches firing.
- 3 corpus triplets; every row well-formed AND queryable.
- `σ_kg ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the retrieval / provenance / hop /
  corpus manifests as predicates.
- **v299.1 (named, not implemented)** is real AEVS-style
  source-linked triplets wired into the v115 σ-memory
  embedding store, a retrieval planner that picks the shortest
  chain whose `σ_total` stays below `τ_warning`, and a corpus
  ingestion pipeline that turns the 80-paper σ-corpus into a
  queryable KG served under `cos kg query`.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

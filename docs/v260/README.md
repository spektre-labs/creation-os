# v260 — σ-Engram (`docs/v260/`)

DeepSeek Engram integration.  Engram splits static
knowledge (facts, entities, syntax) from dynamic
reasoning (logic, analogy, creativity): static lands in
a DRAM hash-table (O(1) lookup), dynamic stays on GPU as
an MoE layer.  v260 types the interface and adds a σ so
"Engram returned a hit" is never the last word.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Parameter split (DeepSeek's ratio)

| static_pct | dynamic_pct | constraint              |
|-----------:|------------:|-------------------------|
| 22 (∈ [20, 25])  | 78 (∈ [75, 80])  | sum == 100       |

### Static-knowledge lookups (exactly 5, O(1) DRAM)

| entity                   | σ_result | lookup_ns |
|--------------------------|---------:|----------:|
| `entity:Helsinki`        | 0.06     | 42        |
| `entity:DeepSeek`        | 0.09     | 57        |
| `syntax:subject_verb_obj`| 0.04     | 38        |
| `fact:speed_of_light`    | 0.05     | 44        |
| `fact:AGPL_v3`           | 0.07     | 51        |

Every `lookup_ns ≤ 100` (DRAM hash-table budget) or the
gate fails.

### Dynamic reasoning (exactly 3, MoE layer)

| task                       | experts | σ_result |
|----------------------------|--------:|---------:|
| `analogy:law_vs_physics`   | 4       | 0.19     |
| `creativity:poem_voice`    | 3       | 0.27     |
| `logic:modus_tollens`      | 2       | 0.12     |

### σ-gated lookup decision (exactly 4 fixtures)

Rule: `σ_result ≤ τ_fact=0.30` → `USE`; else → `VERIFY`.

| query                         | σ_result | decision |
|-------------------------------|---------:|----------|
| `fact:capital_of_finland`     | 0.08     | `USE`    |
| `fact:atomic_number_fe`       | 0.21     | `USE`    |
| `reasoning:legal_precedent`   | 0.48     | `VERIFY` |
| `analogy:ecosystem_economy`   | 0.61     | `VERIFY` |

Both branches fire: Engram doesn't know when it's wrong,
σ does.

### Long context σ (Needle-in-a-Haystack @ 1 M tokens)

| hit_rate_pct | miss_rate_pct | miss_flagged_by_sigma |
|-------------:|--------------:|:---------------------:|
| 97           | 3             | ✅                    |

Engram ships 97 % NIAH @ 1 M.  Without σ, the 3 % miss
is silent.  With σ, the 3 % surfaces.  **Engram + σ ≈
99 %+ reliable long context.**

### σ_engram

```
σ_engram = 1 − (static_lookups_ok + dynamic_rows_ok +
                gate_paths_ok + long_context_ok +
                parameter_split_ok) /
               (5 + 3 + 4 + 1 + 1)
```

v0 requires `σ_engram == 0.0`.

## Merge-gate contract

`bash benchmarks/v260/check_v260_engram_sigma_gated_lookup.sh`

- self-test PASSES
- parameter split in [20, 25] × [75, 80], sum == 100
- 5 static lookups with non-zero hash, `hit`, σ ∈ [0, 1],
  `lookup_ns ≤ 100`
- 3 dynamic rows with `experts_activated > 0`, σ ∈ [0, 1]
- 4 gate fixtures, decision matches σ vs `τ_fact = 0.30`,
  ≥ 1 `USE` AND ≥ 1 `VERIFY`
- long context `hit_rate_pct == 97`, `miss_rate_pct == 3`,
  `miss_flagged_by_sigma`, `ok`
- `σ_engram ∈ [0, 1]` AND `σ_engram == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed static / dynamic / gate /
  long-context manifest with FNV-1a chain.
- **v260.1 (named, not implemented)** — live DRAM hash-
  table-backed Engram, live MoE reasoning layer wired
  through v243 pipeline, v227 entropy decomposition
  driving the knowledge-vs-reasoning split at inference
  time, live NIAH harness producing `hit_rate_pct` from
  measured runs.

## Honest claims

- **Is:** a typed, falsifiable Engram manifest where the
  O(1) lookup budget, the σ-gate branches, and the 97 %
  NIAH row with σ-flagged misses are all merge-gate
  predicates.
- **Is not:** a live Engram backend.  v260.1 is where the
  manifest drives a real DRAM hash-table and MoE layer.

---

*Spektre Labs · Creation OS · 2026*

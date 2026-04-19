# v281 — σ-Jamba (`docs/v281/`)

Hybrid Transformer · Mamba · MoE with σ on every
layer.  AI21's Jamba composes three layer kinds in a
fixed schedule and ships a 2.5× throughput win on
long contexts.  Creation OS types the σ-layer on top:
layer types and complexity are canonical, per-context
architecture choice is σ-adaptive, the 5-tier memory
hierarchy (engram · mamba · ttt · transformer · moe)
is a slot permutation, and the unified bench
predicts σ_jamba ≤ σ_baseline on every dimension.

v281 does not run a hybrid engine.  It types the
σ-layer as four falsifiable manifests.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Layer types (exactly 3, canonical order)

Canonical names AND canonical complexities AND all
three distinct.

| name          | complexity |
|---------------|:----------:|
| `mamba`       | LINEAR     |
| `transformer` | QUADRATIC  |
| `moe`         | SPARSE     |

### Adaptive mixing (exactly 4 contexts, canonical order)

Per-context chosen arch.  Contract: ≥ 2 distinct
archs chosen across contexts (a regression that
always picks one arch fails).

| label     | σ_difficulty | chosen       |
|-----------|-------------:|:------------:|
| `easy`    | 0.10         | MAMBA        |
| `hard`    | 0.80         | TRANSFORMER  |
| `factual` | 0.40         | MOE          |
| `long`    | 0.30         | MAMBA        |

### Memory hierarchy (exactly 5 tiers, canonical order)

Slot permutation equals `[1, 2, 3, 4, 5]` exactly;
every tier `enabled == true`.

| name          | enabled | tier_slot |
|---------------|:-------:|----------:|
| `engram`      | true    | 1         |
| `mamba`       | true    | 2         |
| `ttt`         | true    | 3         |
| `transformer` | true    | 4         |
| `moe`         | true    | 5         |

### Unified bench (exactly 3 metrics, canonical order)

`sigma_jamba ≤ sigma_baseline` per metric — σ-Jamba
is at least as well-calibrated as plain Jamba on
every dimension.  This is a σ-calibration contract,
not a measured throughput claim.

| metric       | sigma_baseline | sigma_jamba |
|--------------|---------------:|------------:|
| `accuracy`   | 0.20           | 0.12        |
| `latency`    | 0.25           | 0.15        |
| `throughput` | 0.18           | 0.10        |

### σ_jamba

```
σ_jamba = 1 − (layer_rows_ok + layer_complexity_ok +
               layer_distinct_ok +
               mixing_rows_ok + mixing_distinct_ok +
               memory_rows_ok + memory_order_ok +
               bench_rows_ok + bench_monotone_ok) /
              (3 + 1 + 1 + 4 + 1 + 5 + 1 + 3 + 1)
```

v0 requires `σ_jamba == 0.0`.

## Merge-gate contract

`bash benchmarks/v281/check_v281_jamba_hybrid_adaptive.sh`

- self-test PASSES
- 3 layer rows canonical (mamba LINEAR / transformer
  QUADRATIC / moe SPARSE), all distinct
- 4 mixing rows canonical; chosen arch per canonical
  mapping; ≥ 2 distinct archs
- 5 memory tiers canonical; slot permutation
  `[1,2,3,4,5]`; all enabled
- 3 bench rows canonical; `sigma_jamba ≤
  sigma_baseline` per metric
- `σ_jamba ∈ [0, 1]` AND `σ_jamba == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed layer / mixing / memory
  / bench manifest with FNV-1a chain.
- **v281.1 (named, not implemented)** — live
  Jamba-style hybrid engine wired into v262 with
  σ-adaptive layer routing per token, measured
  accuracy / latency / throughput vs the
  fixed-schedule baseline, and a unified σ-memory
  bus across all five tiers.

## Honest claims

- **Is:** a typed, falsifiable σ-Jamba manifest where
  layer complexity, σ-adaptive arch choice, memory
  tier permutation, and per-metric σ calibration are
  merge-gate predicates.
- **Is not:** a running hybrid engine, nor a
  measurement of 2.5× throughput or AURCC on long
  contexts.  v281.1 is where the manifest meets real
  layer routing.

---

*Spektre Labs · Creation OS · 2026*

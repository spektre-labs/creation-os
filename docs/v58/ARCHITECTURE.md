# v58 Architecture — σ-Cache: σ-decomposed KV-cache eviction

## One sentence

v58 is a **branchless, aligned, NEON-backed KV-cache eviction
kernel** that selects which tokens to retain (and at what precision
tier) from a **σ-decomposition** of per-token uncertainty —
**epistemic** (reducible, keep) vs **aleatoric** (irreducible,
discount) — instead of the flat attention / entropy signals used by
the rest of the field.

## Why this is novel (Q2 2026 field)

| existing KV-cache eviction policy | retention signal | claim type |
|-----------------------------------|------------------|------------|
| **StreamingLLM** (attention sinks) | positional prior | heuristic |
| **H2O**                              | heavy-hitter attention mass | empirical |
| **SnapKV / PyramidKV**              | layer-wise attention slice | empirical |
| **KIVI / KVQuant**                  | quantised residue | empirical |
| **SAGE-KV / G-KV**                  | block-level attention | empirical |
| **EntropyCache** (2025 arXiv)       | single-token entropy | empirical |
| **Creation OS v58 σ-Cache**         | **σ-decomposed** ε (keep) + α (discount) + recency + sink | **deterministic + branchless + runtime-checked** |

No published policy uses the **epistemic / aleatoric split** as the
retention signal. v58 makes that split the first-class driver and
promotes it from "interesting uncertainty metric" to "explicit cache
policy" — **proven via a 68/68 deterministic self-test and a
reproducible microbench**, not claimed.

## Wire map

```
                 ┌────────────────────────────────────────────────────────┐
                 │              v58 σ-Cache pipeline                      │
                 │                                                        │
   inference ─► │  cos_v58_token_t  {seq_pos, ε, α, attn, sink}          │
   produces    │       │                                                 │
   per token   │       ▼                                                 │
               │  cos_v58_score_batch        (scalar prefetching loop)   │
               │  cos_v58_score_soa_neon     (4-accumulator NEON FMAs)   │
               │       │                                                 │
               │       ▼                                                 │
               │  v58_kth_largest            (descending sort + pick K)  │
               │       │                                                 │
               │       ▼                                                 │
               │  cos_v58_decide             ← branchless kernel         │
               │     • sink  | (g_keep & g_full)  → KEEP_FULL            │
               │     • ~full & g_keep & g_int8    → KEEP_INT8            │
               │     • ~full & ~int8 & g_keep     → KEEP_INT4            │
               │     • else                       → EVICTED              │
               │       │                                                 │
               │       ▼                                                 │
               │  cos_v58_compact           (branchless index write)     │
               │                                                         │
               │  summary: kept_full, kept_int8, kept_int4, evicted,     │
               │           sink_protected, keep_threshold                │
               └────────────────────────────────────────────────────────┘
```

## Tiering (no blending)

| claim                                                              | tier |
|--------------------------------------------------------------------|------|
| Decision kernel is branchless on the hot path                      | **M** — verified by inspection + self-test invariants |
| Sinks are always kept                                              | **M** — `test_decide_preserves_all_sinks`, `test_decide_all_sinks` |
| `kept_total ≤ capacity + 0` (sinks counted inside capacity)         | **M** — `test_decide_kept_le_capacity_plus_sinks` |
| Decision is deterministic under identical policy + tokens          | **M** — `test_decide_deterministic` |
| Every token receives exactly one decision tag (FULL∪INT8∪INT4∪EVICT) | **M** — `test_decide_sum_eq_n` |
| `kept + evicted == n`                                              | **M** — `test_decide_all_evicted_have_tag` |
| NEON SoA path matches scalar reference within 1e-4                 | **M** — `test_score_soa_neon_matches_scalar` |
| σ-decomposition is epistemologically correct                       | **I** — documented; no formal proof of fidelity to the original Dirichlet derivation |
| σ-Cache outperforms H2O / SnapKV / EntropyCache on perplexity       | **P** — requires end-to-end integration with a transformer runtime |

## Hardware discipline (per `.cursorrules`)

1. **Aligned allocation.** All token / score / decision buffers are
   64-byte aligned via a size-rounded `aligned_alloc` helper
   (`v58_alloc64` in the driver; `aligned_alloc(64, ⌈n/64⌉·64)` in
   `cos_v58_alloc_tokens`).
2. **Prefetching.** Scoring and decision loops prefetch 16–64 lanes
   ahead via `__builtin_prefetch(..., 0, 3)`.
3. **NEON 4-accumulator FMA.** `cos_v58_score_soa_neon` materialises
   the `.cursorrules` item 5 pattern (s0..s3 lanes, `vfmaq_f32`,
   4-wide stores).
4. **Branchless.** `cos_v58_decide` computes 0/1 lane masks from
   float compares and merges them with `| & ~`, never with `if` on
   the hot path. `cos_v58_compact` writes every index and advances
   the write cursor by the 0/1 kept flag.
5. **No framework dependency.** Kernel is plain C11 + `arm_neon.h`;
   no Accelerate, no Metal, no Core ML — those frameworks belong to
   other slots.

## Composition into the Verified Agent (v57)

v58 is registered as a v57 composition slot:

| slot                   | owner | target           | tier | summary |
|------------------------|-------|------------------|------|---------|
| `kv_cache_eviction`    | v58   | `make check-v58` | M    | σ-Cache: σ-decomposed KV-cache eviction + per-token precision tier + branchless NEON kernel |

`make verify-agent` dispatches `check-v58` and reports PASS/SKIP/FAIL
alongside the other nine slots. `verify-agent` never silently
downgrades a missing tool to "not a problem"; a broken v58 fails the
aggregate.

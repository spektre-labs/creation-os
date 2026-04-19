# v266 — σ-Flash (`docs/v266/`)

FlashAttention fused with σ.  The attention kernel that
keeps `softmax(Q·K^T)` in SRAM computes `σ_attention =
entropy(softmax(Q·K^T))` in the same pass — strict
sub-1 % overhead — then feeds σ into KV-cache eviction
(high-σ first) and long-context pruning.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Heads (exactly 8, all fused)

Every head: `σ_head ∈ [0, 1]`, `overhead_pct ∈ [0, 1)`
(strict sub-1 % over raw FlashAttention), `fused ==
true`.  A regression that splits σ into a second pass
immediately fails the `fused` check AND likely the
`overhead_pct < 1.0` check.

### Platform kernels (exactly 3, canonical order)

| backend       | latency_ns | sigma_fused |
|---------------|-----------:|:-----------:|
| `cuda_sm90`   |  60        | ✅          |
| `metal_m4`    |  80        | ✅          |
| `neon_arm64`  | 220        | ✅          |

σ-fusion runs on GPU (CUDA), Apple Silicon (Metal) and
CPU (NEON).

### σ-aware KV cache (exactly 6 entries)

Rule: `evict_rank` is the **descending** order of
`σ_kv` — high σ evicted first, low σ kept longest.

| key             | σ_kv | evict_rank |
|-----------------|-----:|-----------:|
| `tok_noise`     | 0.58 | 1          |
| `tok_misspell`  | 0.45 | 2          |
| `tok_the`       | 0.33 | 3          |
| `tok_attention` | 0.19 | 4          |
| `tok_of`        | 0.12 | 5          |
| `tok_hello`     | 0.07 | 6          |

### Long-context σ-pruning (before / after)

| scenario | kept_tokens | effective_ctx_k |
|----------|------------:|----------------:|
| before   | 4 096       | 4               |
| after    | 4 096       | 32              |

Same memory footprint, 8× effective context.  Pruning
drops high-σ tokens so the remaining tokens stretch
further.

### σ_flash

```
σ_flash = 1 − (heads_ok + platforms_ok + kv_rows_ok +
               kv_order_ok + pruning_ok) /
              (8 + 3 + 6 + 1 + 1)
```

v0 requires `σ_flash == 0.0`.

## Merge-gate contract

`bash benchmarks/v266/check_v266_flash_attention_sigma_fused.sh`

- self-test PASSES
- 8 heads, every `fused`, `overhead_pct < 1.0`
- 3 platforms in canonical order, all supported + fused
- 6 KV entries; `evict_rank` permutation of [1..6] in
  descending σ order
- pruning: same `kept_tokens`, strictly larger
  `effective_ctx_k`, `pruning_ok`
- `σ_flash ∈ [0, 1]` AND `σ_flash == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed head / platform / KV /
  pruning manifest with FNV-1a chain.
- **v266.1 (named, not implemented)** — live CUDA /
  Metal / NEON kernels emitting per-head σ in the same
  pass, PagedAttention wired with σ_kv-driven eviction,
  σ-pruning driven by live v226 attention feed over a
  real long-context run.

## Honest claims

- **Is:** a typed, falsifiable FlashAttention + σ
  manifest where σ-fusion overhead, KV eviction order,
  and long-context pruning ratio are merge-gate
  predicates.
- **Is not:** a shipping CUDA kernel.  v266.1 is where
  the manifest drives real GPU / Metal / NEON cores.

---

*Spektre Labs · Creation OS · 2026*

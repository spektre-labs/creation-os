# v267 — σ-Mamba (`docs/v267/`)

State-space backends (Mamba, RWKV) are linear-time
attention alternatives that win dramatically on long
context but are weaker at in-context learning.  v267
picks the backend per query with σ-gated fallback and
provides a Jamba-style interleaved Mamba + Transformer
stack.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Backends (exactly 3, canonical order)

| name          | complexity | exponent | throughput (rel) |
|---------------|------------|---------:|-----------------:|
| `mamba`       | O(n)       | 1        | 5.0              |
| `rwkv`        | O(n)       | 1        | 4.6              |
| `transformer` | O(n²)      | 2        | 1.0              |

`throughput (rel)` is relative to a transformer
baseline on long context.  Contract:
mamba/rwkv.exponent == 1 AND
transformer.exponent == 2 AND
mamba/rwkv.throughput > transformer.

### σ-gated routing (exactly 4 fixtures, τ_mamba = 0.40)

Rule: `σ_mamba ≤ τ_mamba` → `mamba`; else →
`transformer`.  Both branches fire.

| query            | σ_mamba | backend      |
|------------------|--------:|--------------|
| `ctx_summarize`  | 0.18    | mamba        |
| `ctx_retrieve`   | 0.26    | mamba        |
| `ctx_multistep`  | 0.55    | transformer  |
| `ctx_reasoning`  | 0.71    | transformer  |

### Jamba-style hybrid layers (exactly 8)

Alternating `mamba, transformer, mamba, transformer,
…`.  4 mamba layers (linear, long context) + 4
transformer layers (quadratic, in-context learning).
σ per layer selects the better arch automatically.

### Per-task backend comparison (exactly 3 tasks)

Rule: `σ_chosen ≤ σ_rival` for each task AND ≥ 2
distinct chosen backends across the 3 tasks.

| task                  | chosen       | rival       | σ_chosen | σ_rival |
|-----------------------|--------------|-------------|---------:|--------:|
| `longctx_summarize`   | mamba        | transformer | 0.14     | 0.29    |
| `incontext_reason`    | transformer  | mamba       | 0.18     | 0.47    |
| `sequence_tag`        | rwkv         | transformer | 0.16     | 0.24    |

### σ_mamba_kernel

```
σ_mamba_kernel = 1 − (backends_ok + routes_ok +
                      both_routes_ok + layers_ok +
                      layer_split_ok + tasks_ok +
                      task_diversity_ok) /
                     (3 + 4 + 1 + 8 + 1 + 3 + 1)
```

v0 requires `σ_mamba_kernel == 0.0`.

## Merge-gate contract

`bash benchmarks/v267/check_v267_mamba_sigma_gated_fallback.sh`

- self-test PASSES
- 3 backends canonical with exponent + throughput
  contract
- 4 routes at `τ_mamba = 0.40`; both branches fire
- 8 hybrid layers alternating `mamba, transformer`
- 3 tasks with `σ_chosen ≤ σ_rival` AND ≥ 2 distinct
  chosen backends
- `σ_mamba_kernel ∈ [0, 1]` AND `σ_mamba_kernel == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed backend / route / layer /
  task manifest with FNV-1a chain.
- **v267.1 (named, not implemented)** — live SSM /
  RWKV implementations via v262 hybrid engine,
  Jamba-style alternation running real layers,
  σ-gated per-query fallback to a loaded transformer
  kernel.

## Honest claims

- **Is:** a typed, falsifiable Mamba / RWKV /
  Transformer routing manifest where both branches,
  hybrid layer split, and backend diversity are
  merge-gate predicates.
- **Is not:** a running state-space inference kernel.
  v267.1 is where the manifest drives real SSM /
  transformer layers.

---

*Spektre Labs · Creation OS · 2026*

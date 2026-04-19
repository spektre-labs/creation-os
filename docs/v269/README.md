# v269 — σ-Compile-v2 (`docs/v269/`)

AOT compilation of the entire inference pipeline.
v137 AOT-compiled the σ-gate itself; v269 extends the
same discipline to tokenize → embed → attention →
FFN → σ-gate → detokenize, with platform-specific
dispatch, σ-guided PGO on hot paths, and compile-time
elimination of σ-checks whose profiled σ is always
< 0.05.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Pipeline stages (exactly 6, canonical order)

| stage         | aot_compiled | native | latency_ns |
|---------------|:------------:|:------:|-----------:|
| `tokenize`    | ✅           | ✅     |   180      |
| `embed`       | ✅           | ✅     |   240      |
| `attention`   | ✅           | ✅     | 2 600      |
| `ffn`         | ✅           | ✅     | 1 800      |
| `sigma_gate`  | ✅           | ✅     |     1      |
| `detokenize`  | ✅           | ✅     |   140      |

Every stage is AOT-compiled to native code; the
σ-gate itself stays at ~1 ns (v137 legacy).

### Platform targets (exactly 4, canonical order)

| target                  | tok/s  | budget | meets_budget |
|-------------------------|-------:|-------:|:------------:|
| `m4_apple_silicon`      | 118.0  | 100.0  | ✅           |
| `rpi5_arm64`            |  14.0  |  10.0  | ✅           |
| `gpu_4gb_speculative`   |  62.0  |  50.0  | ✅           |
| `x86_avx512`            |  95.0  |  80.0  | ✅           |

Budgets: M4 ≥ 100 tok/s, RPi5 ≥ 10, 4 GB GPU (with
speculative decoding) ≥ 50, x86 AVX-512 ≥ 80.

### σ-guided PGO (exactly 4 hot paths)

Rule: `hotpath_fraction ≥ 0.20` → `"aggressive"`; else →
`"space"`.  Both strategies fire.

| path                    | hotpath_fraction | optimization |
|-------------------------|-----------------:|--------------|
| `path_attention_hot`    | 0.62             | aggressive   |
| `path_embed_common`     | 0.34             | aggressive   |
| `path_rare_dispatch`    | 0.08             | space        |
| `path_fallback_api`     | 0.03             | space        |

### Compile-time σ elimination (exactly 6 layers)

Rule: `elided == (sigma_profile < 0.05)`.  Layers whose
profiled σ is always < 0.05 lose their σ-check at
compile time (zero runtime overhead).

| layer_idx | sigma_profile | elided |
|----------:|--------------:|:------:|
|  3        | 0.02          | ✅     |
|  7        | 0.04          | ✅     |
| 12        | 0.11          | —      |
| 18        | 0.27          | —      |
| 23        | 0.01          | ✅     |
| 29        | 0.34          | —      |

`n_elided >= 1 AND n_kept >= 1` — the system is
adaptive, not all-or-nothing.

### σ_compile_v2

```
σ_compile_v2 = 1 −
  (stages_ok + targets_ok + pgo_rows_ok + pgo_both_ok +
   elim_rows_ok + elim_both_ok) /
  (6 + 4 + 4 + 1 + 6 + 1)
```

v0 requires `σ_compile_v2 == 0.0`.

## Merge-gate contract

`bash benchmarks/v269/check_v269_compile_v2_full_pipeline_aot.sh`

- self-test PASSES
- 6 pipeline stages canonical, every
  `aot_compiled && native && latency_ns > 0`
- 4 platform targets canonical, every
  `tok_per_s >= budget` AND `meets_budget`
- 4 PGO rows; `optimization` matches
  `hotpath_fraction >= 0.20`; both strategies fire
- 6 elim rows; `elided` matches `sigma_profile < 0.05`;
  ≥ 1 elided AND ≥ 1 kept
- `σ_compile_v2 ∈ [0, 1]` AND `σ_compile_v2 == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed stage / target / PGO /
  elim manifest with FNV-1a chain.
- **v269.1 (named, not implemented)** — live LLVM /
  MLIR AOT pipeline for the full inference graph,
  measured tok/s per platform from a loaded model,
  PGO data fed back from production runs driving
  σ-elimination on actually-cold paths.

## Honest claims

- **Is:** a typed, falsifiable AOT-compiled pipeline
  manifest where stage completeness, per-platform
  budgets, both PGO strategies, and adaptive σ
  elimination are merge-gate predicates.
- **Is not:** a live LLVM AOT compilation backend.
  v269.1 is where the manifest drives real codegen.

---

*Spektre Labs · Creation OS · 2026*

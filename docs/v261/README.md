# v261 — σ-AirLLM (`docs/v261/`)

Layer-by-layer inference with a σ per layer.  AirLLM
streams one layer of a 70B model at a time into a 4 GB
GPU (load → compute → release).  v261 types the surface
and adds:

1. a σ per layer (problem layer = unique argmax σ)
2. σ-driven **selective precision** — 4-bit / 8-bit /
   16-bit picked by σ band
3. a hardware-agnostic backend list
4. a speed / abstention tradeoff table where σ-gated
   `aircos` strictly beats raw AirLLM and the human-
   review loop on *effective* tokens/s.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Layers (exactly 8)

| # | σ_layer | precision_bits | is_problem |
|---|--------:|---------------:|:----------:|
| 0 | 0.08    | 4              |            |
| 1 | 0.12    | 4              |            |
| 2 | 0.18    | 4              |            |
| 3 | 0.27    | 8              |            |
| 4 | 0.35    | 8              |            |
| 5 | 0.58    | 16             | ✅         |
| 6 | 0.31    | 8              |            |
| 7 | 0.14    | 4              |            |

Rules:

- `σ_layer ≤ 0.20` → 4-bit
- `0.20 < σ_layer ≤ 0.40` → 8-bit
- `σ_layer > 0.40` → 16-bit (full)

`is_problem` is the unique argmax of `σ_layer` — layer 5
with σ = 0.58 is the one to diagnose.

### Hardware backends (exactly 4)

| name             | supported | min_ram_gb |
|------------------|:---------:|-----------:|
| `cuda_4gb_gpu`   | ✅        | 4          |
| `cpu_only`       | ✅        | 8          |
| `macos_mps`      | ✅        | 8          |
| `linux_cuda`     | ✅        | 12         |

AirLLM is hardware-agnostic by design: 4 GB GPU, CPU-only,
macOS, Linux — v261 types each row.

### Tradeoff regimes (exactly 3)

| regime   | tokens_per_s (effective) | abstain_pct |
|----------|-------------------------:|------------:|
| `slow`   | 0.7                      | 0           |
| `aircos` | 0.9                      | 8           |
| `human`  | 0.3                      | 0           |

Contract: `aircos` has the **strictly highest** effective
tokens/s AND `abstain_pct > 0`.  The σ-gate's teeth
(abstain on high-σ tokens) *saves* wall-clock time by
skipping the human-review loop.

### σ_airllm

```
σ_airllm = 1 − (layers_ok + precision_rule_ok +
                backends_ok + tradeoff_ok + problem_ok) /
               (8 + 8 + 4 + 3 + 1)
```

v0 requires `σ_airllm == 0.0`.

## Merge-gate contract

`bash benchmarks/v261/check_v261_airllm_layer_sigma.sh`

- self-test PASSES
- 8 layers with strictly ascending indices (0..7),
  `σ_layer ∈ [0, 1]`, `precision_bits` matches the
  σ-driven rule
- exactly 1 `is_problem` AND it equals argmax `σ_layer`
- 4 backends, every supported, every `min_ram_gb ≥ 4`
- 3 tradeoff regimes, `aircos` strictly highest tokens/s,
  `abstain_pct > 0`, `tradeoff_ok`
- `σ_airllm ∈ [0, 1]` AND `σ_airllm == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed layer / backend / tradeoff
  manifest with FNV-1a chain.
- **v261.1 (named, not implemented)** — live AirLLM
  layer streamer wired through v243 pipeline, on-disk
  layer cache, runtime precision-switching driven by a
  live σ feed, multi-backend auto-detection at boot.

## Honest claims

- **Is:** a typed, falsifiable AirLLM manifest where the
  selective-precision rule, the problem-layer identifier,
  and the σ-gated tradeoff winner are merge-gate
  predicates.
- **Is not:** a running 70B inference on 4 GB GPU.
  v261.1 is where the manifest drives the live AirLLM
  layer streamer.

---

*Spektre Labs · Creation OS · 2026*

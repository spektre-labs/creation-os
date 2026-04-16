# v46 — σ-optimized BitNet pipeline (lab)

**Evidence class:** portable **O(V) σ-from-logits** helpers + SIMD reductions + adaptive quant **policy** (`make check-v46`). **Not** an in-tree `bitnet.cpp` integration or wall-clock “<3% overhead” claim until `make bench-v46-e2e` produces archived JSON.

## What ships

| Piece | Role |
|------|------|
| `src/v46/fast_sigma.c` | `v46_fast_sigma_from_logits` — Dirichlet `epistemic` / `aleatoric` via `sigma_decompose_dirichlet_evidence`, softmax entropy + top-margin blend into `total` (lab dashboard scalar) |
| `src/v46/sigma_simd.c` | NEON (AArch64) or AVX2 (x86) **sum/max reductions** where available (bandwidth helpers); softmax entropy still uses stable scalar `exp`/`log` loops today |
| `src/v46/adaptive_quant.c` | `v46_sigma_adaptive_quant` — epistemic→bits/sparsity toy policy |
| `benchmarks/v46/SPEED_TABLE.md` | Positioning table with explicit **I/M/N** discipline |

## Verify

```bash
make check-v46
make bench-v46-e2e
```

## Working paper title (only when harness exists)

**“σ-Optimized BitNet Serving: Near-Zero-Margin σ Telemetry on CPU-Efficient Ternary Inference”**

Thesis sentence: keep **one forward** to produce logits; spend only **O(V) scans** for σ telemetry; treat “honesty” as a **gated policy** measurable via abstention / calibration-gap harnesses (v44–v45), not as a slogan.

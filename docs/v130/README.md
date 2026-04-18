# v130 — σ-Codec

**Adaptive compression for peer communication (FP4 LoRA · σ-profile pack · PQ embed · σ-aware context).**

## Problem

v128 mesh + v129 federated move data constantly.  Without a dedicated codec every LoRA delta is 10s of MB, every v126 embedding is 10 kB, every context blob is kilobytes of text.  On mobile / satellite links this kills the system.

## σ-innovation

Four sub-codecs, all pure C:

### A. FP4 LoRA-delta packer (8× vs fp32)
Signed-magnitude, 4 bits/value with a shared per-tensor scale.
```
code = round(|x| / scale · 7)
bit7 = sign(x)
```
On ~Gaussian LoRA deltas the round-trip rel-RMSE stays around **0.04** (measured in `check-v130`).

### B. σ-profile packer (4× vs fp32, 8 bytes / profile)
8 channels in [0,1] ↔ 8 uint8.  Max round-trip error ≤ 1/255 ≈ 0.004.

### C. Product Quantization for 2568-d v126 embeddings (1284× vs fp32)
```
M = 8 subspaces × sub_dim = 321    (2568 = 8·321)
K = 128 codes per subspace
→ 8 bytes / vector
```
Deterministic mini-Lloyd with SplitMix64-seeded init.  Recon cosine > **0.90** on synthetic low-rank data.

### D. σ-aware context compression
Each chunk's character budget is allocated as
```
weight_i = 0.5 + σ_i
alloc_i  = round(base_budget · weight_i / Σ weight)
```
so **high-σ (uncertain) chunks keep more detail** — the downstream reasoner needs that context to make sense of the uncertainty.  Low-σ chunks get terse summaries.

## Contract

```c
cos_v130_quantize_fp4    (float*, n, packed)      → scale
cos_v130_dequantize_fp4  (packed, n, scale, out)
cos_v130_pack_sigma      (channels[8], codes[8])
cos_v130_unpack_sigma    (codes[8], channels[8])
cos_v130_pq_train        (cb, vectors, n, iters, seed)
cos_v130_pq_encode       (cb, vec, codes[8])
cos_v130_pq_decode       (cb, codes[8], out_vec)
cos_v130_compress_context(chunks, n, budget, out, cap)
```

## What's measured by `check-v130`

- FP4: rel-RMSE < 0.10 on 1024 synthetic LoRA values.
- σ-profile pack: max round-trip error ≤ 1/255.
- PQ: train on 200 low-rank vectors, recon cosine > 0.90.
- σ-context: total output < total input **and** the high-σ chunk's key word survives after truncation of the low-σ chunks.

## What ships now vs v130.1

| v130.0 (this commit) | v130.1 (follow-up) |
|---|---|
| FP4 / σ-pack / PQ / σ-context (pure C, no deps) | Entropy coder (zstd or ANS) stacked on top |
| Linear scale FP4 (simple, fast) | Non-linear E2M1-style quant for long-tailed deltas |
| Synthetic benchmarks in merge-gate | Real BitNet hidden-state corpus + recall numbers |
| CLI probes | Wire into v128 mesh peer-to-peer payload path |

v130.0 is framework-free — no zlib, no lz4 — so the merge-gate can validate round-trip fidelity on any runner.

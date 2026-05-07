# Creation OS — Hardware setup and performance notes

This page is a **practical integrator guide** for local inference with σ-gate and `cos chat`. **Tokens-per-second** figures below are **illustrative community / vendor-style ranges**, not Creation OS harness numbers. For measured microbenches, use `make bench` and archive host metadata per [`REPRO_BUNDLE_TEMPLATE.md`](REPRO_BUNDLE_TEMPLATE.md) and [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).

## Recommended stack: Apple Silicon (M4 Pro / Max)

σ-gate + local model + `cos chat`, all on-box.

### Minimum

- **M4 Pro, ~24 GB unified memory**: MoE-class quantized weights (for example **Q4_K_M**) at rough **tens of tokens/s** in common community reports for small active-parameter footprints.
- Install: `pip install creation-os[chat]`
- Example **llama.cpp** server (OpenAI-compatible):  
  `llama-server -hf <org>/<repo-GGUF>:<quant> --port 8001`  
  (pick a concrete revision and quant from the card publisher; **Q4_K_M** is a common “good speed vs quality” default.)
- Client: `cos chat --endpoint http://localhost:8001/v1`

### Optimal

- **M4 Max, ~64 GB**: room for larger dense or MoE quants (for example **Q4_K_M** / **Q5_K_M** tiers).
- **Flash attention** (when your build supports it): e.g. `--flash-attn` on compatible `llama-server` builds.
- **Speculative decoding**: draft + target, e.g. `llama-server -m <large.gguf> -md <small.gguf>` (syntax per your `llama.cpp` version).
- **MLX**: Some Apple Silicon setups report **higher throughput than llama.cpp** for certain model sizes and builds (often cited on the order of **~20–87%** for **&lt;14B-class** models in **informal** comparisons). **Verify on your machine**; do not treat that interval as a Creation OS benchmark.

### Maximum

- **Very large unified memory** (for example **~192 GB** class): headroom for **70B+** class models at stronger quants (for example **Q6_K**) when the runtime supports them.
- **MLX-LM** example server: `pip install mlx-lm` then e.g. `mlx_lm.server --model <mlx-community/your-4bit-or-8bit>` (see upstream docs for flags).

## NVIDIA GPU

- **RTX 4090 (24 GB)**: large models often need aggressive quant or hybrid offload; **Q4_K_M** is a common compromise.
- **RTX 5090 (32 GB)**: more headroom for stronger quants or larger context.
- **vLLM** (example): `vllm serve <model> --dtype auto --gpu-memory-utilization 0.9` (see vLLM docs for your stack).

## CPU-only (development / smoke)

- **x86_64**, **~16 GB RAM** typical minimum for **~7B-class Q4_K_M** at low tokens/s (highly CPU/Cores dependent).
- Example: `llama-server -m model-q4km.gguf --port 8001`

## Quantization (rule of thumb)

| Format | ~size (7B class) | Quality (informal) | Speed (informal) |
|--------|------------------|--------------------|------------------|
| Q8_0   | ~7.5 GB          | high               | slower           |
| Q6_K   | ~5.5 GB          | strong             | fast             |
| Q5_K_M | ~5.0 GB          | strong             | fast             |
| Q4_K_M | ~4.0 GB          | good default prod  | very fast        |
| Q3_K_M | ~3.3 GB          | weaker             | very fast        |

**Rule:** Treat **Q4_K_M** as a typical **minimum** for production-quality text unless you have measured acceptance on your task. Very aggressive quants (for example **Q2** or **IQ1** tiers) often degrade quality sharply.

## σ-gate latency (not the generation bottleneck)

- **L1** (entropy / “lite” **σ** in Python): **no** heavy ML deps; typical per-call latency is **sub-millisecond** on a modern laptop for short strings — not the main cost next to LLM decode.
- **L2–L5** / **LSD** probes: require the matching optional stack; latency often **~10–50 ms** per score on GPU-class hosts depending on probe and batch (order-of-magnitude lab guidance only).

## Speculative decoding and σ (integration pattern)

**llama.cpp** speculative mode uses a small **draft** and a large **verify** model. A **creation-side** pattern is: score draft fragments with the σ-gate; **ACCEPT**-band outcomes can reduce how often you pay for full verification on the target model, while **RETHINK** / **ABSTAIN** bands steer back to verification or abstention. Wire this in your agent or server shim; it is **not** enabled automatically inside stock `llama-server` alone.

Use `cos hardware` (or `from cos.hardware import HardwareInfo`) for a **local** RAM/GPU hint and conservative model-tier suggestions.

## See also

- [`LOCAL_MODEL_SETUP.md`](LOCAL_MODEL_SETUP.md) — `cos chat` endpoints (llama.cpp, Ollama, LM Studio, OpenAI-compatible).
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — σ layers and thresholds.

# Creation OS v46 — speed / memory / energy table (positioning + tier discipline)

This file is a **communication scaffold**: headline competitor numbers are **external literature / vendor claims** unless an archived harness row exists in-repo.

## Tier legend (per cell)

- **M** — measured in this repository (command + artifact path in `docs/WHAT_IS_REAL.md`)
- **I** — imported / external reference (paper, vendor blog, third-party bench); not reproduced here by default
- **P** — projected / modeled from partial data; not a shipped measurement
- **N** — not claimed in-repo until harness + JSON exist

## CPU-only positioning table (illustrative)

| Model | Memory | Latency / token | Energy / token | σ-gating | Honest? |
|:--|:--|:--|:--|:--:|:--:|
| BitNet 2B + σ (Creation OS v46 lab) | 0.4 GB (**I**) | 29.3 ms (**I** base) + σ scans (**N** until `make bench-v46-e2e` JSON) | 0.028 J (**I**) | YES (**M** math path) | YES (**N** until abstention harness rows) |
| BitNet 2B (vanilla) | 0.4 GB (**I**) | 29.0 ms (**I**) | 0.028 J (**I**) | no | no |
| Qwen2.5 1.5B Q4 | 1.4 GB (**I**) | 41 ms (**I**) | 0.186 J (**I**) | no | no |
| Llama 3.2 1B Q4 | 1.1 GB (**I**) | 58 ms (**I**) | 0.264 J (**I**) | no | no |
| Gemma-3 1B Q4 | 1.3 GB (**I**) | 67 ms (**I**) | 0.347 J (**I**) | no | no |
| Phi-4-mini 3.8B Q4 | 2.8 GB (**I**) | 124 ms (**I**) | 0.649 J (**I**) | no | no |

## Notes (honest)

- **“Honest”** here means: the system can **abstain / gate** when σ thresholds fire; Creation OS documents that as a **policy + harness** surface (v33/v44), not an automatic property of weights alone.
- **σ overhead:** v46 ships **O(V) logits scans** + optional SIMD reductions (`src/v46/sigma_simd.c`). A wall-clock **<3%** headline is **N** until `benchmarks/v46/*.json` contains pinned host metadata (CPU model, freq governor, compiler flags, tokenizer, batch=1).
- **BitNet numbers** in the BitNet rows are treated as **I-tier** imports until reproduced under `docs/REPRO_BUNDLE_TEMPLATE.md`.

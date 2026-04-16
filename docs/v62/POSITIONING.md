# v62 Reasoning Fabric — positioning

## What everyone else ships in 2026

| System / paper                            | What it is                                                                       | What it does **not** ship                                                  |
|-------------------------------------------|----------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| **Coconut** (arXiv:2412.06769)            | Continuous latent CoT — *training method* + paper                                | Branchless C kernel; M4 NEON path; composition with security gate.         |
| **EBT** (arXiv:2507.02092, ICLR 2026)     | Energy-Based Transformer — *training method* + ICLR paper                        | Standalone energy verifier kernel callable from any inference stack.       |
| **HRM** (arXiv:2506.21734, sapientinc)    | Hierarchical Reasoning Model — 27 M-param model + research code                  | Pure C, NEON, branchless H/L loop reusable on top of any base model.       |
| **NSA** (arXiv:2502.11089, DeepSeek)      | Native Sparse Attention — Triton kernels for A100 / H100                         | Apple-silicon-aligned C path; no CUDA dependency.                          |
| **DeepSeek-V3 MTP**                       | Multi-Token Prediction — *training* objective + draft heads                      | Branchless C draft + verify pair under one ABI.                            |
| **ARKV** (arXiv:2603.08727)               | Adaptive KV management — paper + reference code                                  | Branchless 3-state classifier you can drop into a CPU runtime.             |
| **Claude Code / Codex CLI / Cursor CLI**  | Beautiful proprietary CLIs over remote frontier models                           | Single-binary, no-deps, runs on `cos` against *local* kernels.             |
| **Aider / Bubbletea / Ratatui apps**      | Open-source TUIs in Go / Rust                                                    | Pure C, ~500 LOC, no runtime, NO_COLOR-respecting, Apple-HIG output.       |

## What v62 + cos ship that nobody else does

> The **first open-source local AI runtime to land the 2026 reasoning
> frontier (Coconut + EBT + HRM + NSA + MTP + ARKV) as one branchless
> C kernel**, composed with a runtime security kernel (v60 σ-Shield) and
> a formal-lattice attestation kernel (v61 Σ-Citadel), behind a single
> Apple-tier CLI front door (`cos`).

Concretely:

* **One ABI for six modules.** `src/v62/fabric.h` is the single header
  any inference stack can bind to. No Triton, no CUDA, no Python, no
  framework.
* **Branchless on M4.** Every hot loop respects the rules in
  `.cursorrules` — 64-byte aligned alloc, NEON 4-way, prefetch, no
  conditional branches in the inner loop, no `fread`.
* **Composed safety + reasoning.** `cos_v62_compose_decision` is a 3-bit
  branchless AND of (σ-Shield action gate, Σ-Citadel lattice gate, EBT
  energy gate). No reasoning step emits without all three lanes ALLOW.
* **Apple-tier CLI.** `cli/cos.c` is ~500 lines of C, no dependencies,
  honors `NO_COLOR` and `COS_NO_UNICODE`, isatty-detects, and prints in
  the Apple HIG style: clarity over decoration, content over chrome.

## Comparison table (one screen)

| Property                                        | Claude Code | Cursor CLI | Aider | llama.cpp | MLX-LM | **Creation OS (v60+v61+v62 + cos)** |
|-------------------------------------------------|:-----------:|:----------:|:-----:|:---------:|:------:|:-----------------------------------:|
| Local execution                                 |     ⨯       |    ⨯       |   ✓   |     ✓     |   ✓    |               ✓                     |
| Open source                                     |     ⨯       |    ⨯       |   ✓   |     ✓     |   ✓    |               ✓                     |
| Single-binary, no deps                          |     ⨯       |    ⨯       |   ⨯   |     ✓     |   ⨯    |               ✓                     |
| Runtime security kernel (cap + σ-gate)          |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v60)               |
| Formal-lattice + attestation kernel             |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v61)               |
| Latent CoT kernel                               |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v62)               |
| Energy-based verifier kernel                    |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v62)               |
| Hierarchical H/L loop kernel                    |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v62)               |
| Native Sparse Attention kernel (3-branch)       |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v62)               |
| Multi-Token Predictor draft + verify            |     ⨯       |    ⨯       |   ⨯   |  partial  |   ⨯    |               ✓ (v62)               |
| Adaptive ORIG/QUANT/EVICT KV manager            |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (v62)               |
| Composed σ-Shield + Σ-Citadel + EBT decision    |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓                     |
| DARPA-CHACE 12-layer security gate              |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓ (`make chace`)      |
| SLSA-3 keyless OIDC provenance                  |     ⨯       |    ⨯       |   ⨯   |     ⨯     |   ⨯    |               ✓                     |

## Honest non-claims (so the σ-check is clean)

* v62 is **kernels, not weights**. It is the alien-tier *shape* of the
  2026 frontier in C; production model weights stay in MLX / llama.cpp /
  bitnet.cpp.
* The Apple-tier CLI is **not a chat UI**. It is a status board, a
  verifier, a kernel-runner, and a demo of the composed decision. Chat
  belongs in the model layer.
* SME (Apple M4 matrix engine) is **compile-only** today, gated by
  `COS_V62_SME=1`. The default build is NEON and never SIGILLs.

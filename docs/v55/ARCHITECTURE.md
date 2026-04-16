# Creation OS v55 — σ₃-Speculative: Architecture

**Tier:** scaffold (C) — deterministic proxies on caller-supplied softmax output.
**No network**, **no inference engine**, **no tokenizer**.
Binary: `creation_os_v55`  ·  Tests: `make check-v55` → **29/29 PASS**.

## Wire map

```
            ┌────────────────────────────────────────────────┐
            │                caller (yours)                  │
            │   transformer forward pass → logits / probs    │
            └────────────┬───────────────────────────┬───────┘
                         │                           │
                 [target dist.]              [draft dist.]
                         │                           │
                         ▼                           ▼
            ┌────────────────────┐     ┌────────────────────┐
            │  src/v55/sigma3.c  │     │  src/v55/sigma3.c  │
            │  v55_sigma3_t      │     │  v55_sigma3_t      │
            │   σ_input          │     │   σ_input          │
            │   σ_knowledge      │─┐ ┌─│   σ_knowledge      │
            │   σ_decoding       │ │ │ │   σ_decoding       │
            │   σ_total          │ │ │ │   σ_total          │
            └────────────────────┘ │ │ └────────────────────┘
                                   ▼ ▼
                     ┌───────────────────────────────┐
                     │         src/v55/ears.c        │
                     │  v55_ears_accept(...)         │
                     │    r < min(1,                 │
                     │       P_t/P_d + α·σ_know)     │
                     └─────────┬─────────────────────┘
                               │  (if α=0  ⇒ vanilla spec-decode)
                               ▼
                     ┌───────────────────────────────┐
                     │         src/v55/easd.c        │
                     │  v55_easd_decide(...)         │
                     │    reject ≡                   │
                     │       H_t ≥ τ ∧ H_d ≥ τ       │
                     │     ∧ overlap_K ≥ ρ           │
                     └─────────┬─────────────────────┘
                               │
                               ▼
                       {accept, reject, force-resample}
```

## Three 2026 insights, coded into C11

| Component | Source | Role in v55 |
|:--|:--|:--|
| **σ₃ decomposition** | Taparia, Senanayake, Thopalli, Narayanaswamy — *The Anatomy of Uncertainty in LLMs* (arXiv:2603.24967, March 2026) | Semantic framing: σ = **σ_input + σ_knowledge + σ_decoding**, replacing the classical aleatoric/epistemic dichotomy. v55 implements **deterministic proxies** on a single forward pass; swap in a learned head without API changes. |
| **EARS** | Sun, Mao, Xu, Chen — *Efficient Adaptive Rejection Sampling for Accelerating Speculative Decoding* (arXiv:2512.13194, Dec 2025) | Threshold-relaxation rule: `accept iff r < min(1, P_t/P_d + α·σ_knowledge)`. Reported **+18.12 % throughput**, −0.84 % accuracy (GSM8K). Fixes the "random rejection" pathology when the target model is itself uncertain. |
| **EASD** | Su, Zhang, He — *Entropy-Aware Speculative Decoding Toward Improved LLM Reasoning* (arXiv:2512.23765, Dec 2025; ICLR 2026 under review) | Quality gate on top of spec-decode: `reject iff H_t ≥ τ ∧ H_d ≥ τ ∧ overlap_K ≥ ρ`. Reported **+3.54 % accuracy** on reasoning benchmarks with near-equivalent throughput. |

## σ₃ — deterministic proxies

With only a softmax output in hand we can't *learn* the Taparia decomposition. We can, however, define deterministic proxies that are **reproducible, falsifiable, and hardware-friendly**:

| Component | Proxy definition | Intuition |
|:--|:--|:--|
| **σ_input** | Normalized Gini dispersion over the top-*K* probabilities: `1 − Σ(p_i/S)²` rescaled by `1 − 1/K`. | Prompt-level ambiguity leaks into the output as multiple competing top candidates. Single dominant top-*K* → 0; uniform top-*K* → 1. |
| **σ_knowledge** | `1 − max(P)` (same signal EARS exploits). | Target model's own self-confidence on its top choice. |
| **σ_decoding** | `H / log₂(N)`, normalized Shannon entropy over the full distribution. | Sampling-level randomness. Bounded [0, 1] by construction. |

`σ_total` is the mean of the three proxies, clamped to [0, 1]. See `src/v55/sigma3.h` for the full API.

## EARS — branchless acceptance

```c
threshold = min( max_threshold, (P_target / P_draft) + α · σ_knowledge )
accept    = (rnd < threshold)
```

Key properties encoded in the self-test:

- **α = 0 ⇒ identical to vanilla spec-decode.** `v55_ears_threshold(…, α=0, …) = P_t / P_d`.
- **Asymmetric uncertainty relaxes the threshold.** When target σ_knowledge is high, the threshold can rise above `P_t / P_d` — exactly the "random rejection" regime EARS was designed to fix.
- **Invariant preserved:** the threshold is always clamped at `max_threshold` (1.0), so acceptance probability never exceeds 1.
- **Batch helper** writes 0/1 into an `int*` mask, no allocations; the body is auto-vectorisable on aarch64 and x86 (no branches, only FP compares + min).

## EASD — entropy-aware quality gate

EASD is **composable on top of EARS** (they solve opposite problems):

- **EARS** fixes *random rejection* when uncertainty is **asymmetric** (target unsure, draft plausible).
- **EASD** fixes *random acceptance* when uncertainty is **symmetric** AND the distributions **agree on who's plausible** — i.e. both models are confidently-wrong about the same candidates.

Decision logic (all three conditions required; any one false ⇒ pass through):

1. `H_target / log₂(N)  ≥  h_thresh`  (target not confident)
2. `H_draft  / log₂(N)  ≥  h_thresh`  (draft not confident either)
3. `|topK_t ∩ topK_d| / K  ≥  overlap_thresh`  (they agree on the fuzz)

The self-test encodes the opposite regime explicitly: **high entropy but disagreeing top-*K*** → **no reject**. That's the scenario where uncertainty is *informative* (an ensemble signal) and we want the underlying spec-decode to proceed.

## Hardware path (M4-first)

Per `.cursorrules` and `creation-os-m4-silicon.mdc`:

- **NEON SIMD hot entropy loop** (`src/v55/sigma3.c`): `vld1q_f32` loads, four parallel `vmlaq_f32` accumulators, `__builtin_prefetch(&p[i+64], 0, 3)` ahead of each 16-lane iteration.
- **Branchless fast log₂** via IEEE-754 exponent extraction + second-order minimax polynomial. ±0.01 bits accuracy across (0, ∞) — sufficient for σ-gating. No libm call on the hot path; compiler lowers the bit fiddling to a pair of shifts and an `FADD`.
- **Scalar fallback** (bit-identical on non-ARM) ensures the self-test is reproducible in CI without aarch64 hardware.
- **64-byte alignment** is *caller-owned*: `v55_sigma3_from_logits` takes a caller-provided scratch pointer, no `malloc` on the hot path. Self-test uses stack buffers.
- **`-march=native`** on aarch64 activates NEON for free; there is no separate `+sme` build flag because the hot path is dot-product-sized, not matmul-sized — SME2 is deferred to layer repulsion (see `native_m4/`).

## Composition — how v55 fits the rest of Creation OS

| Layer | What v55 adds |
|:--|:--|
| **v34** — aleatoric/epistemic decomposition | v55 offers an alternative decomposition (σ₃); both are compile-time options, not exclusive. |
| **v35** — σ-guided speculative decoding | v55 provides **two concrete, branchless acceptance rules** (EARS, EASD) that the v35 harness can plug in. |
| **v45** — σ-introspection | `v55_sigma3_t` exposes four scalars per step; plugs directly into a calibration-gap measurement. |
| **v46** — σ-optimized BitNet pipeline | v55's NEON entropy loop reuses the same 4-accumulator template as v46's σ SIMD reductions — zero structural drift. |
| **v54** — σ-proconductor | When v54 dispatches a query across subagents, each response's `sigma_knowledge` can be computed cheaply with v55 before the σ-weighted aggregation step. |

## Claims discipline

- **This binary does not benchmark spec-decode throughput.** It certifies the **math and policy** of EARS + EASD on deterministic synthetic cases. Throughput numbers cited above are from the papers, not from our self-test.
- **σ₃ proxies are proxies, not probes.** Semantically they follow Taparia 2026; numerically they are single-forward-pass lower bounds. A learned Taparia-style head would replace `v55_sigma3_from_probs` behind the same interface.
- Tier tags live in [`docs/WHAT_IS_REAL.md`](../WHAT_IS_REAL.md); this file is the architecture map, not the receipt.

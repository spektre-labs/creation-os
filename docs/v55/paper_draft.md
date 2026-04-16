# σ₃-Speculative: Branchless C11 Wiring of Three-Component Uncertainty, EARS, and EASD

**Spektre Labs · Helsinki**
**Position paper · v55 · draft**

## Abstract

We report a 5-file, 29-test C11 scaffold (`src/v55/`, binary `creation_os_v55`) that wires three recent (Dec 2025 – Mar 2026) uncertainty-aware inference insights into a single branchless policy layer: the three-component σ decomposition proposed by Taparia et al. (2026), the EARS adaptive acceptance rule of Sun et al. (2025), and the EASD entropy-aware quality gate of Su et al. (2025). We argue these three results are naturally **composable** — EARS fixes random rejection when uncertainty is asymmetric, EASD fixes random acceptance when uncertainty is symmetric, and σ₃ supplies the signal to distinguish the two regimes in a single forward pass. Our implementation is hardware-first (NEON 4-accumulator entropy loop, branchless fast log₂ via IEEE-754 exponent trick + 2nd-order minimax polynomial, 64-byte caller-aligned scratch, scalar fallback bit-identical on non-ARM) and deterministic (29/29 self-test on synthetic distributions). We make no throughput claims; we certify the **math, policy, and hardware discipline** of the acceptance surface.

## 1. Motivation

Speculative decoding accelerates autoregressive LLM inference by letting a small **draft** model propose tokens that a large **target** model verifies in parallel. Standard rejection sampling accepts a draft token `x` with probability `min(1, P_t(x) / P_d(x))`. Two failure modes have been identified in the recent literature:

- **Random rejection** (Sun et al., 2025 — arXiv:2512.13194). When the target is *itself* uncertain, plausible draft candidates are rejected by a fixed threshold. EARS introduces a tolerance proportional to `1 − max(P_t)`: `r < min(1, P_t/P_d + α · σ_knowledge)`. Reported: **+18.12 %** throughput with −0.84 % accuracy on GSM8K.

- **Random acceptance** (Su, Zhang, He, 2025 — arXiv:2512.23765). When *both* models are uncertain **and** their top-N candidates overlap, standard rejection sampling silently accepts low-confidence errors and propagates them. EASD intercepts this regime by forcing a target resample. Reported: **+3.54 %** accuracy on reasoning benchmarks with near-equivalent throughput.

A separate line of work (Taparia, Senanayake, Thopalli, Narayanaswamy, 2026 — arXiv:2603.24967) argues the classical **aleatoric / epistemic** decomposition is too coarse for LLMs and proposes three semantic components: **input ambiguity**, **knowledge gap**, and **decoding randomness**.

These three results are, to our reading, strongly complementary. EARS needs `σ_knowledge`; EASD needs `σ_decoding`; the composition needs a way to tell which regime a step is in. σ₃ names the three axes.

## 2. Contribution

v55 implements:

1. **Deterministic proxies for σ₃** computable on a single softmax output:
   - `σ_input` = normalized Gini dispersion over top-*K* probabilities
   - `σ_knowledge` = `1 − max(P)`
   - `σ_decoding` = `H / log₂(N)`

2. **Branchless EARS acceptance** in ~10 lines of C:
   ```c
   threshold = min(max_threshold, P_t/P_d + α · σ_knowledge);
   accept    = (rnd < threshold);
   ```

3. **Branchless EASD decision** (three-condition AND, all FP compares):
   ```c
   reject = (H_t ≥ τ) ∧ (H_d ≥ τ) ∧ (overlap_K/K ≥ ρ);
   ```

4. **A NEON 4-accumulator entropy hot path** with a branchless fast log₂ approximation (IEEE-754 exponent + 2nd-order minimax poly, ±0.01 bits) and `__builtin_prefetch(&p[i+64], 0, 3)`. Scalar fallback is bit-identical on non-ARM for CI reproducibility.

5. **29 deterministic self-tests** including: `α = 0 ⇔ vanilla spec-decode`; threshold clamp at `max_threshold`; high entropy **without** top-K overlap → no EASD reject (disagreement is informative); asymmetric uncertainty (target sure, draft unsure) → EARS regime not EASD.

## 3. Why this is a position paper, not a benchmark

We deliberately do **not** report the EARS / EASD throughput or accuracy numbers. Those require a specific draft/target model pair, a pinned dataset (GSM8K, reasoning benchmarks), and a spec-decode harness. The v55 scaffold is the **policy underneath such a harness**, not the harness itself. Separating the policy from the harness lets us:

- Formally test the acceptance math against deterministic synthetic cases.
- Keep `src/v55/` network-free (see `creation.md` Invariant #3).
- Expose a stable C ABI for a future bitnet.cpp / vLLM integration to consume.

## 4. Composition with the Creation OS σ stack

| Prior layer | What v55 gives it |
|:--|:--|
| v34 — aleatoric/epistemic σ₂ | Alternative σ₃ decomposition, selectable at compile time. |
| v35 — σ-guided speculative decoding | Concrete acceptance math (EARS) + a quality gate (EASD). |
| v45 — σ-introspection | Three sub-questions for the "does it know it doesn't know?" test. |
| v46 — σ-optimized BitNet pipeline | Same 4-accumulator NEON template, zero structural drift. |
| v54 — σ-proconductor | Cheap per-response `σ_knowledge` for σ-weighted aggregation. |

## 5. Relation to existing uncertainty / selective prediction work

- **Evidential deep learning (EDL)** predicts Dirichlet distributions for joint aleatoric/epistemic separation in a single forward pass. σ₃ extends this axis set to three semantic components; the v55 proxies are lower bounds that do not require an EDL-trained head.
- **Conformal prediction** calibrates thresholds on a held-out set. EARS's `α · σ_knowledge` tolerance is a per-step relaxation that does not require a calibration set, complementary to post-hoc conformalization.
- **Selective prediction** asks whether to answer at all. v55 asks a finer question: **given that we're answering, which acceptance rule should fire?**

## 6. Reproducibility

```
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make check-v55          # 29/29 PASS in ~1 s on aarch64
./creation_os_v55 --architecture
```

All inputs to the self-test are inlined in `src/v55/creation_os_v55.c`. There are no weights, no datasets, no network dependencies.

## 7. Honest scope

v55 is a **scaffold (tier C)** in the `docs/WHAT_IS_REAL.md` taxonomy. It certifies the math and the policy; it does not certify spec-decode throughput on a real engine. Future work: wire v55's acceptance functions into a bitnet.cpp draft/target pair and report the expected +15–20 % throughput window with the tests that the v55 scaffold already enforces on the policy layer.

## References

- Taparia A., Senanayake R., Thopalli K., Narayanaswamy V. *The Anatomy of Uncertainty in LLMs.* arXiv:2603.24967, March 2026.
- Sun C., Mao A., Xu L., Chen M. *Efficient Adaptive Rejection Sampling for Accelerating Speculative Decoding in Large Language Models.* arXiv:2512.13194, December 2025.
- Su T., Zhang M., He G. *Entropy-Aware Speculative Decoding Toward Improved LLM Reasoning.* arXiv:2512.23765, December 2025 (ICLR 2026 under review).
- Leviathan Y., Kalman M., Matias Y. *Fast Inference from Transformers via Speculative Decoding.* ICML 2023.
- Chen C. et al. *Accelerating Large Language Model Decoding with Speculative Sampling.* arXiv:2302.01318, 2023.

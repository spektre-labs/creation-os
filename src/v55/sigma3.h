/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v55 σ₃ — three-component uncertainty decomposition (scaffold).
 *
 * Framing: Taparia et al., "The Anatomy of Uncertainty in LLMs"
 * (arXiv:2603.24967, March 2026) argue the classical aleatoric /
 * epistemic split is too coarse for generative models and propose
 * three semantic components:
 *
 *   σ_input       — input ambiguity (ambiguous prompt)
 *   σ_knowledge   — knowledge gap (insufficient parametric evidence)
 *   σ_decoding    — decoding randomness (stochastic sampling)
 *
 * Creation OS interpretation (scaffold / tier C): we expose
 * **deterministic proxies** computable from a single forward pass
 * output distribution, so the decomposition is reproducible and
 * falsifiable without additional forward passes or prompt-classifier
 * networks:
 *
 *   σ_input      := top-K probability dispersion (normalized Gini over
 *                   top-K).  High when multiple plausible continuations
 *                   compete (prompt-level ambiguity leaking into the
 *                   output).
 *   σ_knowledge  := 1 - max(P).  Same signal EARS uses for spec-decode
 *                   threshold relaxation (arXiv:2512.13194, Dec 2025).
 *   σ_decoding   := H / log2(N), normalized Shannon entropy of the full
 *                   distribution.  Bounded in [0, 1].
 *
 * These are **deterministic functions of the softmax output** — they
 * are not a learned Dirichlet model.  We use the Taparia 2026
 * framework as the semantic anchor; caller can substitute any
 * pre-trained uncertainty head without API changes.
 *
 * Hot path uses NEON SIMD on aarch64 (vld1q/vmulq/vaddq) with a
 * branchless fast log₂ approximation; scalar fallback is bit-identical
 * on non-ARM.
 */
#ifndef CREATION_OS_V55_SIGMA3_H
#define CREATION_OS_V55_SIGMA3_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float sigma_input;       /* top-K dispersion (normalized Gini), [0,1] */
    float sigma_knowledge;   /* 1 - max(P), [0,1] */
    float sigma_decoding;    /* H / log2(N), [0,1] */
    float sigma_total;       /* bounded aggregate in [0,1] (mean of components) */
    float h_bits;            /* raw Shannon entropy in bits */
    int   top_k_used;        /* K actually used for sigma_input */
} v55_sigma3_t;

/* Numerically stable softmax in place. */
void v55_softmax_inplace(float *logits, int n);

/* Compute σ₃ from an already-softmaxed probability vector. */
void v55_sigma3_from_probs(const float *probs, int n, int top_k, v55_sigma3_t *out);

/* Compute σ₃ from raw logits using a caller-provided scratch buffer of
 * at least n floats (avoids malloc on the hot path; aligned_alloc
 * recommended for the caller's scratch). */
void v55_sigma3_from_logits(const float *logits, int n, int top_k,
                            float *scratch_probs, v55_sigma3_t *out);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V55_SIGMA3_H */

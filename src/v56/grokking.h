/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v56 grokking — commutator-defect σ-channel from Singular Learning
 * Theory (SLT).
 *
 * Framing: "Grokking as a Phase Transition between Competing
 * Basins: a Singular Learning Theory Approach" (arXiv:2603.01192,
 * March 2026) and "Why Grokking Takes So Long" (arXiv:2603.13331)
 * establish that **commutator defects** between consecutive
 * gradient steps run 10–1000× higher inside grokking transitions
 * than in non-grokking controls, with onset 600–1600 steps before
 * generalization and a power-law dependence on learning rate.
 *
 * Creation OS interpretation: the commutator defect is a new,
 * orthogonal σ-channel — it measures the *structural* uncertainty
 * of the weight trajectory rather than the output distribution.
 * High defect + stable loss ⇒ "about to grok" (fruitful
 * uncertainty); high defect + rising loss ⇒ "going off-manifold"
 * (destructive uncertainty).
 *
 * v56 exposes a scaffold-tier C implementation that operates on a
 * stream of gradient-proxy vectors (the caller may pass real
 * gradients, activation deltas, or any quantity whose
 * step-to-step non-alignment is meaningful).  Hot path uses NEON
 * 4-accumulator reduction; scalar fallback is bit-exact on
 * non-ARM.
 */
#ifndef CREATION_OS_V56_GROKKING_H
#define CREATION_OS_V56_GROKKING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float spike_multiplier;   /* defect > baseline × this ⇒ phase_transition (e.g. 10.0) */
    int   baseline_warmup;    /* steps of warm-up before spike detection arms */
    float baseline_ewma;      /* EWMA coeff for baseline (e.g. 0.98) */
} v56_grok_params_t;

typedef struct {
    float baseline_defect;           /* slow-moving baseline */
    float latest_defect;             /* most recent (normalized) defect */
    float max_defect_observed;       /* running max */
    int   step_count;
    int   phase_transition_armed;    /* 1 after warmup completed */
    int   phase_transition_detected; /* 1 iff latest > baseline × spike_multiplier */
    int   transitions_total;         /* total number of detections */
} v56_grok_state_t;

/* Normalized commutator-defect proxy for two gradient-proxy vectors:
 *   defect = ||g_new − g_prev||² / (||g_new||² + ||g_prev||² + ε)
 * Bounded in [0, 1].  0 = perfectly aligned (no rotation), 1 =
 * orthogonal/reversed (full phase rotation of the update direction).
 * Exposed for unit testing; also called internally by
 * v56_grok_observe. */
float v56_grok_defect(const float *g_new, const float *g_prev, int dim);

void v56_grok_init(const v56_grok_params_t *p, v56_grok_state_t *s);
void v56_grok_observe(const v56_grok_params_t *p,
                      v56_grok_state_t *s,
                      const float *g_new,
                      const float *g_prev,
                      int dim);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V56_GROKKING_H */

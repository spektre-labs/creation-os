/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Continual — catastrophic-forgetting protection (ATLAS / SSU live).
 *
 * v278 RSI and the P6 TTT primitive both write to fast weights while
 * the model is running.  Without a brake, the fast weights drift
 * until old knowledge is lost (catastrophic forgetting).  This
 * primitive is the brake.
 *
 * For every parameter we keep a running "importance" score, a proxy
 * for Fisher information:
 *
 *     importance_i ← decay · importance_i + (1 − decay) · grad_i²
 *
 * Parameters whose importance exceeds ``freeze_threshold`` are
 * considered critical to retained knowledge.  Before the TTT update
 * is applied, we *mask out* the gradient entries for frozen params:
 *
 *     grad_i ← 0   if importance_i > freeze_threshold
 *
 * This is the SSU (ICLR 2026) shape in 30 lines of C: the model
 * learns on the free parameters while the protected parameters stay
 * pinned to the old distribution.
 *
 * Companion behaviour (``σ-replay``) lives in the orchestrator; the
 * primitive here is pure — it only asks "what did my own gradients
 * say was important?" and produces a mask.
 *
 * Contracts (v0):
 *   1. init() zeros importance, rejects NULL buffers / n ≤ 0 /
 *      freeze_threshold ≤ 0 / decay ∉ [0, 1].
 *   2. observe_gradient(grad) updates importance and returns the
 *      number of parameters that BECAME frozen on this step.
 *   3. apply_mask(grad) zeros grad[i] whenever importance_i >
 *      freeze_threshold.  Returns the count of zeros written.
 *   4. step(grad) = observe + apply_mask in that order; importance
 *      is updated with the ORIGINAL gradient, then the mask is
 *      applied to the caller's buffer.  n_steps / frozen_count are
 *      kept on the state for observability.
 *   5. decay_tick() multiplies importance by ``decay`` — useful for
 *      calibrating "we haven't seen old data in a while, let
 *      importance fade".
 *   6. Float NaN / inf in grad is treated as zero (does not poison
 *      importance).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CONTINUAL_H
#define COS_SIGMA_CONTINUAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float   *importance;     /* length n; caller-owned */
    int      n;
    float    freeze_threshold;
    float    decay;          /* default 0.99 */
    int      n_steps;
    int      frozen_count;   /* current # of frozen params */
    uint64_t n_masked_total; /* cumulative zeros written    */
} cos_sigma_continual_t;

int   cos_sigma_continual_init(cos_sigma_continual_t *c,
                               float *importance, int n,
                               float freeze_threshold,
                               float decay);

/* Returns the number of parameters that newly crossed the threshold. */
int   cos_sigma_continual_observe_gradient(cos_sigma_continual_t *c,
                                           const float *grad);

/* Returns the number of gradient entries zeroed. */
int   cos_sigma_continual_apply_mask(cos_sigma_continual_t *c,
                                     float *grad);

/* observe + apply in one call; returns zeros-written. */
int   cos_sigma_continual_step(cos_sigma_continual_t *c, float *grad);

void  cos_sigma_continual_decay_tick(cos_sigma_continual_t *c);

int   cos_sigma_continual_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_CONTINUAL_H */

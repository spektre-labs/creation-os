/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Unlearn — the right-to-be-forgotten, operationalised.
 *
 * GDPR Article 17 (and FIT, arxiv 2601.21682) require controllers
 * to remove a subject's data on request.  For model weights the
 * naive solution is re-training from scratch — expensive and
 * impractical.  σ-Unlearn does it surgically.
 *
 * Model:
 *   - ``weights``        : the live parameter vector (length n).
 *   - ``target_vector`` : the activation pattern the data point
 *                        would elicit (length n).  Think of it as
 *                        "this is what the model looks like when
 *                        recalling this fact".
 *
 * Procedure (projection removal):
 *       coeff = strength · (w · t) / (t · t)
 *       w_i  ← w_i − coeff · t_i
 *
 * With strength = 1 a single pass makes w orthogonal to t; with
 * strength = 0.5 each pass halves the projection, so σ approaches
 * 1 geometrically.  Weights orthogonal to the target are untouched
 * (coeff = 0).  The strength parameter (∈ [0, 1]) controls how
 * aggressive one pass is; fractional values leave headroom to
 * probe σ after each step and stop once σ_target is reached.
 *
 * Verification (the σ in σ-Unlearn):
 *   σ_unlearn = 1 − |cos(weights, target_vector)|
 *
 * σ = 0 means the model still recalls the data perfectly (every-
 * thing points the same direction).  σ = 1 means the model is
 * orthogonal to the target — the fact is gone.  Callers iterate
 * apply() / verify() until σ_unlearn ≥ σ_target (typically 0.9).
 *
 * Contracts (v0):
 *   1. apply() with strength = 0 is a no-op.
 *   2. apply() monotonically reduces |cos(weights, target)| on
 *      aligned inputs.
 *   3. verify() returns σ ∈ [0, 1]; degenerate vectors (all-zero
 *      weights or all-zero target) → σ = 1 (nothing to recall
 *      against = trivially forgotten).
 *   4. iterate() runs apply+verify in a loop until σ ≥ σ_target
 *      OR max_iters exhausted; reports the final σ and iteration
 *      count.
 *   5. FNV-1a-64 hash of a subject id doubles as a request id so
 *      unlearn events are logged deterministically (shared with
 *      the engram primitive's hashing convention).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_UNLEARN_H
#define COS_SIGMA_UNLEARN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t subject_hash;    /* FNV-1a-64 of the subject id */
    float    strength;        /* step size ∈ (0, 1]         */
    int      max_iters;
    float    sigma_target;    /* stop when σ_unlearn ≥ this */
} cos_unlearn_request_t;

typedef struct {
    uint64_t subject_hash;
    int      n_iters;
    float    sigma_before;
    float    sigma_after;
    float    l1_shrunk;       /* Σ |Δweight| over the run   */
    bool     succeeded;       /* σ_after ≥ σ_target         */
} cos_unlearn_result_t;

/* Apply one σ-unlearn pass to ``weights`` in place. */
void  cos_sigma_unlearn_apply(float *weights, const float *target,
                              int n, float strength);

/* Verification σ: 1 − |cos(weights, target)|.  Degenerate vectors
 * (L2 ≤ eps on either side) return 1.0 (nothing to forget against). */
float cos_sigma_unlearn_verify(const float *weights,
                               const float *target, int n);

/* Full loop: apply → verify → repeat until σ ≥ target OR max_iters
 * reached.  Writes stats into ``result``. */
int   cos_sigma_unlearn_iterate(float *weights, const float *target,
                                int n,
                                const cos_unlearn_request_t *req,
                                cos_unlearn_result_t *result);

/* FNV-1a-64, shared with the engram primitive.  Exposed here so
 * callers can build ``subject_hash`` the same way. */
uint64_t cos_sigma_unlearn_hash(const char *s);

int   cos_sigma_unlearn_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_UNLEARN_H */

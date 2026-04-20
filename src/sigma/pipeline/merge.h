/*
 * σ-Merge — σ-gated model merging (slerp / linear / TIES / task-arith).
 *
 * Model merging in 2026 is a crowded shelf: MergeKit, TIES, DARE,
 * task-arithmetic — most of which work on image classifiers and
 * partially on LLMs.  The recent meta-analysis (arxiv 2511.21437,
 * Feb 2026) reports that merging methods *mostly fail on LLMs*.
 *
 * Creation OS does not claim to solve merging.  It measures the
 * merge with σ and refuses to ship a regression:
 *
 *   σ_before_a = σ(model_a, calibration_fixture)
 *   σ_before_b = σ(model_b, calibration_fixture)
 *   σ_after   = σ(merge(a,b,α, method), calibration_fixture)
 *
 * A merge is CONSTRUCTIVE iff σ_after < min(σ_before_a, σ_before_b).
 * Otherwise it is DESTRUCTIVE and gets rejected.
 *
 * alpha-sweep: the runtime walks α ∈ {0.0, 0.25, 0.50, 0.75, 1.0}
 * and picks the α that minimises σ_after.  No gradient, no gradient
 * tape — just integer / float arithmetic on the caller's calibration
 * vectors.  The caller brings:
 *
 *   - two weight vectors, same length
 *   - a σ oracle: given a weight vector, return σ on your benchmark
 *
 * This keeps the primitive weight-format agnostic (BitNet, GGUF, MLX,
 * a row of task-arithmetic deltas — σ-Merge does not care).
 *
 * Methods (v0, all operate per-coordinate):
 *
 *   LINEAR    : m = α · a + (1 − α) · b
 *   SLERP     : m = sin((1−α)Ω)/sinΩ · a  +  sin(αΩ)/sinΩ · b
 *               (falls back to LINEAR when Ω ≈ 0)
 *   TIES      : sign-consensus + top-k by magnitude + α average
 *   TASK_ARITH: m = base + α · (a − base) + (1 − α) · (b − base)
 *               (caller passes `base` as the third vector)
 *
 * Contracts (v0):
 *   1. init rejects NULL vectors, mismatched lengths, or α ∉ [0,1].
 *   2. merge writes the result into caller-owned `out`.
 *   3. sigma oracle is a function pointer; result is clamped to
 *      [0, 1] before comparison.
 *   4. sweep picks the lowest σ_after; ties broken by lowest |α−0.5|
 *      (prefer balanced merges).
 *   5. self-test uses synthetic vectors where the merge arithmetic
 *      is hand-computed; no LLM required.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MERGE_H
#define COS_SIGMA_MERGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_MERGE_LINEAR     = 0,
    COS_MERGE_SLERP      = 1,
    COS_MERGE_TIES       = 2,
    COS_MERGE_TASK_ARITH = 3,
} cos_merge_method_t;

typedef struct {
    cos_merge_method_t method;
    float              alpha;
    /* Optional base vector for TASK_ARITH; may be NULL otherwise.  */
    const float       *base;
    /* TIES: keep only the top-k coordinates by magnitude (per vec)
     * before merging; 0 means "no trim".                          */
    int                ties_top_k;
} cos_merge_config_t;

typedef struct {
    float sigma_before_a;
    float sigma_before_b;
    float sigma_after;
    float best_alpha;
    cos_merge_method_t method;
    int   constructive;   /* σ_after < min(σ_a, σ_b) */
} cos_merge_result_t;

/* σ oracle: given a weight vector of length n, return σ ∈ [0,1]. */
typedef float (*cos_merge_sigma_oracle_fn)(const float *weights, int n,
                                           void *ctx);

/* One-shot merge: writes `out[0..n-1]` from a,b (and optionally base).
 * Returns 0 on success, <0 on invalid inputs. */
int cos_sigma_merge(const float *a, const float *b, int n,
                    const cos_merge_config_t *cfg,
                    float *out);

/* Evaluate once: compute σ_a, σ_b, merged vector, σ_after. */
int cos_sigma_merge_evaluate(const float *a, const float *b, int n,
                             const cos_merge_config_t *cfg,
                             cos_merge_sigma_oracle_fn oracle,
                             void *oracle_ctx,
                             float *scratch_out,
                             cos_merge_result_t *out);

/* α-sweep over {0.0, 0.25, 0.5, 0.75, 1.0} (or caller-supplied grid).
 * Writes the best merged vector to `out_best` and fills `out_result`.
 * `alphas` may be NULL (uses the canonical 5-point grid). */
int cos_sigma_merge_alpha_sweep(const float *a, const float *b, int n,
                                cos_merge_method_t method,
                                const float *base,
                                int ties_top_k,
                                const float *alphas, int n_alphas,
                                cos_merge_sigma_oracle_fn oracle,
                                void *oracle_ctx,
                                float *scratch,
                                float *out_best,
                                cos_merge_result_t *out_result);

int cos_sigma_merge_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_MERGE_H */

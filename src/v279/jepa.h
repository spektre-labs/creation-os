/*
 * v279 σ-JEPA — world model with σ = prediction error.
 *
 *   LeCun's JEPA (2022) and the LeWorldModel paper
 *   (03/2026, "stable end-to-end JEPA from pixels") both
 *   train a predictor in a learned representation space
 *   and stabilise it with a regulariser on the latent
 *   code.  The prediction error itself is the σ of the
 *   world model:
 *     * low σ  → the model knows what happens next → ACT
 *     * high σ → the model does not know         → OBSERVE
 *
 *   v279 types the σ-layer on top of JEPA as four
 *   merge-gate manifests.  v279 does NOT run a JEPA
 *   trainer — the throughput and accuracy of a live
 *   world model are reserved for v279.1.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Prediction gate (exactly 4 fixtures, τ_predict = 0.30):
 *     Each: `step`, `σ_prediction ∈ [0, 1]`,
 *     `decision ∈ {ACT, OBSERVE}`,
 *     rule: `σ_prediction ≤ τ_predict → ACT` else OBSERVE.
 *     Contract: ≥ 1 ACT AND ≥ 1 OBSERVE.
 *
 *   Latent convergence (exactly 3 checkpoints, canonical
 *   order `early`, `mid`, `late`):
 *     Each: `label`, `entropy_z ∈ [0, 1]`,
 *     `sigma_latent ∈ [0, 1]`.
 *     Contract:
 *       - entropy_z strictly decreasing across checkpoints
 *       - sigma_latent strictly decreasing across checkpoints
 *       - for each row |entropy_z − sigma_latent| ≤ 0.05
 *         (entropy minimisation converges with σ
 *         minimisation — they are the same objective).
 *
 *   Loss terms (exactly 2, canonical order):
 *     `prediction` and `regularizer`, each
 *     `enabled == true`, `weight ∈ (0, 1)`.
 *     Contract: both present, distinct names, weights sum
 *     to 1.0 (± 1e-3) — LeWorldModel's two-term loss.
 *
 *   Validation manifest (exactly 2 citations):
 *     `lecun_jepa_2022` (representation-space prediction)
 *     and `leworldmodel_2026_03` (end-to-end stable JEPA);
 *     both present AND distinct sources.
 *     This is a citation contract, not an accuracy claim.
 *
 *   σ_jepa (surface hygiene):
 *       σ_jepa = 1 −
 *         (predict_rows_ok + predict_both_ok +
 *          latent_rows_ok +
 *          latent_monotone_entropy_ok +
 *          latent_monotone_sigma_ok +
 *          latent_converge_ok +
 *          loss_rows_ok + loss_distinct_ok +
 *          loss_weights_ok +
 *          validation_rows_ok + validation_distinct_ok) /
 *         (4 + 1 + 3 + 1 + 1 + 1 + 2 + 1 + 1 + 2 + 1)
 *   v0 requires `σ_jepa == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 prediction rows; decision matches σ vs
 *        τ_predict; both branches fire.
 *     2. 3 latent rows canonical; entropy_z and
 *        sigma_latent both strictly decreasing; per-row
 *        convergence |entropy_z − sigma_latent| ≤ 0.05.
 *     3. 2 loss terms canonical, distinct, weights sum
 *        to 1.0.
 *     4. 2 validation rows; both present, distinct
 *        sources, non-empty evidence.
 *     5. σ_jepa ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v279.1 (named, not implemented): a LeWorldModel
 *     JEPA trainer wired into v262 with σ-gated
 *     action/observe routing, measured latent entropy
 *     against σ on a pixel world model, and a live
 *     prediction-error gate.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V279_JEPA_H
#define COS_V279_JEPA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V279_N_PREDICT     4
#define COS_V279_N_LATENT      3
#define COS_V279_N_LOSS        2
#define COS_V279_N_VALIDATION  2

typedef enum {
    COS_V279_DEC_ACT     = 1,
    COS_V279_DEC_OBSERVE = 2
} cos_v279_dec_t;

typedef struct {
    int             step;
    float           sigma_prediction;
    cos_v279_dec_t  decision;
} cos_v279_predict_t;

typedef struct {
    char  label[8];
    float entropy_z;
    float sigma_latent;
} cos_v279_latent_t;

typedef struct {
    char  name[14];
    bool  enabled;
    float weight;
} cos_v279_loss_t;

typedef struct {
    char  source  [28];
    char  evidence[48];
    bool  present;
} cos_v279_citation_t;

typedef struct {
    cos_v279_predict_t   predict   [COS_V279_N_PREDICT];
    cos_v279_latent_t    latent    [COS_V279_N_LATENT];
    cos_v279_loss_t      loss      [COS_V279_N_LOSS];
    cos_v279_citation_t  validation[COS_V279_N_VALIDATION];

    float tau_predict;    /* 0.30 */
    float converge_eps;   /* 0.05 */

    int   n_predict_rows_ok;
    int   n_predict_act;
    int   n_predict_observe;

    int   n_latent_rows_ok;
    bool  latent_monotone_entropy_ok;
    bool  latent_monotone_sigma_ok;
    bool  latent_converge_ok;

    int   n_loss_rows_ok;
    bool  loss_distinct_ok;
    bool  loss_weights_ok;

    int   n_validation_rows_ok;
    bool  validation_distinct_ok;

    float sigma_jepa;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v279_state_t;

void   cos_v279_init(cos_v279_state_t *s, uint64_t seed);
void   cos_v279_run (cos_v279_state_t *s);

size_t cos_v279_to_json(const cos_v279_state_t *s,
                         char *buf, size_t cap);

int    cos_v279_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V279_JEPA_H */

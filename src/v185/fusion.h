/*
 * v185 σ-Multimodal-Fusion — N-modal σ-weighted fusion.
 *
 *   v118 vision, v127 voice, v184 VLA each live in their own
 *   σ-channel.  v185 lets *any* number of modalities register
 *   with a common projection dimension and fuses them into a
 *   single hidden state whose σ reports both the per-modality
 *   confidence *and* the cross-modal consistency.
 *
 *       project(modality_i : R^d_i)  ─►  R^D
 *       weight_i           = 1 / (1 + σ_i)
 *       fusion             = Σ w_i · p_i  /  Σ w_i
 *       σ_cross            = mean cosine-distance across
 *                            projected modalities
 *       σ_fused            = 1 - Π(1 - σ_i·present)
 *
 *   The exit invariants (exercised in the merge-gate):
 *
 *     1. A consistent multimodal sample (cat image + caption
 *        "cat" + sound "meow") has σ_cross < 0.20.
 *     2. A conflicting sample (cat image + caption "dog")
 *        has σ_cross ≥ 0.50 and the kernel flags it.
 *     3. The modality registry refuses to admit a modality
 *        whose σ_channel > τ_drop; the same modality is
 *        dynamically re-admitted when its σ falls back below.
 *     4. Fusion weights are normalized: Σ w_i / Σ w_i = 1 up
 *        to ULP; weight of a modality with σ=0 dominates.
 *     5. Graceful degradation: dropping the highest-σ modality
 *        *reduces* σ_cross on every test sample where it was
 *        the outlier.
 *
 * v185.0 (this file) ships a deterministic fixture with four
 * registered modalities (vision, audio, text, action) × 16
 * samples where the ground-truth consistency label is known.
 *
 * v185.1 (named, not shipped):
 *   - real SigLIP / Whisper / BitNet / policy-head encoders
 *   - cos modality register --external plugin ABI
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V185_FUSION_H
#define COS_V185_FUSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V185_MAX_MOD         4
#define COS_V185_D               32        /* common projection dim */
#define COS_V185_N_SAMPLES       16
#define COS_V185_STR_MAX         32

typedef struct {
    char  name[COS_V185_STR_MAX];
    char  encoder[COS_V185_STR_MAX];
    int   native_dim;                       /* e.g. 768 (SigLIP) */
    char  sigma_channel[COS_V185_STR_MAX];  /* e.g. "σ_vis" */
    bool  active;                           /* false ⇒ dropped */
    float last_sigma;
} cos_v185_modality_t;

typedef struct {
    int     id;
    int     n_active;
    bool    ground_truth_consistent;        /* fixture label */

    /* Per-modality σ from fixture */
    float   sigma_per[COS_V185_MAX_MOD];
    /* Per-modality projected vector */
    float   proj    [COS_V185_MAX_MOD][COS_V185_D];
    /* Weight used in the fusion */
    float   weight  [COS_V185_MAX_MOD];
    bool    present [COS_V185_MAX_MOD];

    float   fused   [COS_V185_D];
    float   sigma_cross;                    /* mean cos-dist */
    float   sigma_fused;                    /* noisy-OR */
    bool    flagged_conflict;               /* σ_cross ≥ τ */
} cos_v185_sample_t;

typedef struct {
    int                 n_modalities;
    cos_v185_modality_t modalities[COS_V185_MAX_MOD];

    int                 n_samples;
    cos_v185_sample_t   samples[COS_V185_N_SAMPLES];

    float               tau_conflict;       /* σ_cross ≥ ⇒ flag */
    float               tau_drop;           /* σ_i    >  ⇒ drop */
    uint64_t            seed;

    int                 n_consistent_correct;
    int                 n_conflict_correct;
    int                 n_dropped_modalities;
    float               mean_sigma_cross_consistent;
    float               mean_sigma_cross_conflict;
} cos_v185_state_t;

void   cos_v185_init          (cos_v185_state_t *s, uint64_t seed);
void   cos_v185_register      (cos_v185_state_t *s,
                                const char *name,
                                const char *encoder,
                                int   native_dim,
                                const char *sigma_channel);
void   cos_v185_build_samples (cos_v185_state_t *s);
void   cos_v185_run_fusion    (cos_v185_state_t *s);

size_t cos_v185_to_json       (const cos_v185_state_t *s,
                                char *buf, size_t cap);

int    cos_v185_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V185_FUSION_H */

/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v180 σ-Steer — representation engineering under σ control.
 *
 *   Mechanistic interpretability (v179) tells us *what* happens.
 *   v180 steers *what happens next*: a small set of direction
 *   vectors is added to the hidden state at runtime, but only
 *   when σ crosses a threshold.  No retraining, no prompt hacks.
 *
 *   Three steering vectors are shipped in v180.0 as deterministic
 *   fixtures:
 *
 *     truthful   — pulls hidden state away from hallucination
 *                  direction, lowers σ.
 *     no_firmware— suppresses the disclaimer / self-minimization
 *                  feature, reduces firmware-correlated tokens.
 *     expert     — shifts toward dense-lexicon mode when v132
 *                  persona.level = "expert".
 *
 *   Exit invariants (merge-gate):
 *
 *     1. Applying the truthful vector to high-σ samples drops
 *        mean σ_product by ≥ 10 % relative.
 *     2. Steering on low-σ samples leaves σ essentially alone
 *        (|Δσ| ≤ 0.02 mean) — the gate respects the condition.
 *     3. `no_firmware` lowers the firmware-token rate across a
 *        fixed prompt bank by ≥ 25 %.
 *     4. `expert` raises the expert-lexicon score monotonically
 *        as persona confidence increases.
 *     5. Output is byte-deterministic on a fixed seed.
 *
 *   v180.1 (named, not shipped):
 *     - real activation hooks through v101 specialist bridge;
 *     - SAE-derived directions computed from v179 features;
 *     - per-layer α schedules persisted to
 *       `models/v180/steer_truthful.bin` etc.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V180_STEER_H
#define COS_V180_STEER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V180_D              64        /* hidden dim for fixture */
#define COS_V180_N_PROMPTS      48
#define COS_V180_N_VECTORS       3
#define COS_V180_NAME_MAX       32

typedef enum {
    COS_V180_VEC_TRUTHFUL    = 0,
    COS_V180_VEC_NO_FIRMWARE = 1,
    COS_V180_VEC_EXPERT      = 2,
} cos_v180_vec_kind_t;

typedef struct {
    cos_v180_vec_kind_t kind;
    char                name[COS_V180_NAME_MAX];
    float               v[COS_V180_D];     /* unit-norm direction */
    float               alpha;             /* default step size */
} cos_v180_vector_t;

typedef struct {
    int   id;
    float hidden[COS_V180_D];              /* pre-steer hidden state */
    float sigma_before;                    /* baseline σ_product */
    float sigma_after;                     /* σ after steer (if gated) */
    bool  steered;                         /* did the σ-gate fire? */
    float firmware_rate_before;            /* fixture-only */
    float firmware_rate_after;
    float expert_score_before;             /* fixture-only */
    float expert_score_after;
} cos_v180_sample_t;

typedef struct {
    int   n_samples;
    cos_v180_sample_t samples[COS_V180_N_PROMPTS];

    cos_v180_vector_t vectors[COS_V180_N_VECTORS];

    float tau_high_sigma;                  /* σ-gate for truthful */
    float tau_firmware;                    /* σ-gate for no_firmware */
    uint64_t seed;

    /* Aggregate metrics produced by run(). */
    float mean_sigma_before_high;          /* over samples with σ≥τ */
    float mean_sigma_after_high;
    float rel_sigma_drop_high_pct;         /* positive if truthful helps */
    float mean_sigma_abs_delta_low;        /* over low-σ samples */

    float mean_firmware_before;
    float mean_firmware_after;
    float rel_firmware_drop_pct;

    float expert_low_score;                /* persona level 0 */
    float expert_mid_score;                /* persona level 1 */
    float expert_high_score;               /* persona level 2 */
} cos_v180_state_t;

void cos_v180_init(cos_v180_state_t *s, uint64_t seed);
void cos_v180_build_vectors(cos_v180_state_t *s);
void cos_v180_build_samples(cos_v180_state_t *s);
void cos_v180_run_truthful(cos_v180_state_t *s);
void cos_v180_run_no_firmware(cos_v180_state_t *s);
void cos_v180_run_expert_ladder(cos_v180_state_t *s);

size_t cos_v180_to_json(const cos_v180_state_t *s, char *buf, size_t cap);

int cos_v180_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V180_STEER_H */

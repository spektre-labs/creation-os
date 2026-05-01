/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v184 σ-VLA — vision-language-action with σ per stage.
 *
 *   v118 shipped vision, v149 shipped embodied action.  v184
 *   composes them into a dual-system architecture where σ
 *   gates the entire see → understand → act pipeline:
 *
 *       image  ─►  SigLIP encoder (System 2)
 *              ─►  v126 embed    (σ_perception)
 *              ─►  BitNet reason (σ_plan)
 *              ─►  policy head   (σ_action, System 1)
 *              ─►  trajectory    (σ_grounding check)
 *
 *   The exit invariants (exercised in the merge-gate):
 *
 *     1. ≥ 8/10 synthetic grounding queries produce the correct
 *        object bounding box (System 2 perception).
 *     2. Every stage emits a σ in [0,1]; a dual-system product
 *        σ = 1 - (1-σ_p)(1-σ_plan)(1-σ_a) is available.
 *     3. When any stage σ > τ_gate the pipeline *aborts* and
 *        asks a human — no unchecked action is executed.
 *     4. Ambiguous prompts ("Take the red cup" with 3 red cups
 *        visible) trigger the clarification path and raise
 *        σ_grounding above the threshold.
 *
 * v184.0 (this file) ships a deterministic, weights-free
 * fixture: 10 synthetic scenes × 1 target object each, closed-
 * form grounding, closed-form dual-system σ.
 *
 * v184.1 (named, not shipped):
 *   - real SigLIP + Moondream 1.6B + BitNet 2B composite on
 *     RPi5 (v165 edge)
 *   - real policy-head diffusion trajectory
 *   - real v140 causal attribution over action failures
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V184_VLA_H
#define COS_V184_VLA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V184_N_SCENES     10
#define COS_V184_N_CANDS       5
#define COS_V184_STR_MAX      96

typedef struct {
    int   id;
    int   x, y, w, h;
    char  label[COS_V184_STR_MAX];
    float color_r, color_g, color_b;
} cos_v184_box_t;

typedef struct {
    int              id;
    char             prompt[COS_V184_STR_MAX];
    char             target_label[COS_V184_STR_MAX];
    float            target_r, target_g, target_b;

    int              n_candidates;
    cos_v184_box_t   candidates[COS_V184_N_CANDS];
    int              truth_idx;           /* ground-truth candidate */

    /* Predicted grounding */
    int              predicted_idx;
    bool             grounding_correct;
    bool             ambiguous;           /* ≥ 2 candidates ~equally good */
    int              n_plausible;

    /* Per-stage σ */
    float            sigma_perception;
    float            sigma_plan;
    float            sigma_action;
    float            sigma_grounding;
    float            sigma_dual;          /* 1 - Π(1 - σ_i) */

    /* Dual-system trajectory */
    bool             system2_fired;
    bool             system1_fired;
    bool             aborted;             /* any σ > τ */
    bool             asked_human;
} cos_v184_scene_t;

typedef struct {
    int              n_scenes;
    cos_v184_scene_t scenes[COS_V184_N_SCENES];

    float            tau_gate;            /* any σ > this ⇒ abort */
    float            tau_ambiguous;       /* Δcolor below this ⇒ ambiguous */
    uint64_t         seed;

    /* Aggregate metrics */
    int              n_correct;
    int              n_aborted;
    int              n_asked_human;
    int              n_ambiguous;
    float            mean_sigma_dual;
    float            mean_sigma_perception;
    float            mean_sigma_plan;
    float            mean_sigma_action;
    float            mean_sigma_grounding;
} cos_v184_state_t;

void   cos_v184_init  (cos_v184_state_t *s, uint64_t seed);
void   cos_v184_build (cos_v184_state_t *s);
void   cos_v184_run   (cos_v184_state_t *s);

size_t cos_v184_to_json (const cos_v184_state_t *s,
                         char *buf, size_t cap);

int    cos_v184_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V184_VLA_H */

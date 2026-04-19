/*
 * v254 σ-Tutor — adaptive tutoring system.
 *
 *   v252 (Teach) ships a single Socratic session.  v254
 *   ships a full adaptive tutoring system on top of it:
 *   a typed learner model, a σ-steered curriculum, four
 *   teaching modalities with per-modality σ_fit, a
 *   progress tracker, and a privacy-first posture with
 *   an audited data-export path.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Learner model (exactly 4 skills — Bayesian Knowledge
 *   Tracing row):
 *     linear_algebra · calculus · probability ·
 *     discrete_math
 *   Every skill carries:
 *     p_mastery   ∈ [0, 1]
 *     sigma_mastery ∈ [0, 1] (uncertainty of the BKT row)
 *     skill_ok    = (both ∈ [0, 1])
 *   v197 ToM is cited as the upstream learner model.
 *
 *   Curriculum (exactly 4 exercises):
 *     Each exercise has:
 *       skill (one of the 4 above)
 *       difficulty ∈ [0, 1]
 *       learner_level ∈ [0, 1]
 *       σ_difficulty = |difficulty − learner_level|
 *       fit = (σ_difficulty ≤ τ_fit, τ_fit = 0.20)
 *     The v0 fixture ships exactly 4 exercises whose
 *     σ_difficulty is within τ_fit (on-level).  v141
 *     curriculum is cited as the generator.
 *
 *   Multi-modal teaching (exactly 4 modalities):
 *     text · code · simulation · dialog
 *   Each modality carries `σ_fit ∈ [0, 1]` and a
 *   `chosen` flag.  Rule: exactly one modality is
 *   `chosen == true`, and it is the one with the
 *   *minimum* σ_fit in the fixture.  This is the σ-gate
 *   for modality selection.
 *
 *   Progress tracking (exactly 3 per-skill trend rows):
 *     Each row: `skill`, `pct_before`, `pct_after`,
 *     `delta_pct = pct_after − pct_before`.
 *     v0 fixture requires: every `pct_after ≥ pct_before`
 *     AND at least 1 row has `delta_pct > 0` — progress
 *     is real, not narrated.  v172 narrative is cited as
 *     the story layer.
 *
 *   Privacy (v182): 4 required flags, all true:
 *     local_only · no_third_party · user_owned_data ·
 *     export_supported
 *
 *   σ_tutor (surface hygiene):
 *       σ_tutor = 1 −
 *         (skills_ok + exercises_fit +
 *          modality_chosen_ok + progress_rows_ok +
 *          privacy_flags) /
 *         (4 + 4 + 1 + 3 + 4)
 *   v0 requires `σ_tutor == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 4 skills in canonical order, every
 *        `skill_ok == true`.
 *     2. Exactly 4 exercises; every `σ_difficulty ≤ τ_fit`
 *        AND every `fit == true`.
 *     3. Exactly 4 modalities in canonical order; exactly
 *        one `chosen == true` AND it is the modality
 *        with the minimum σ_fit.
 *     4. Exactly 3 progress rows; every `pct_after ≥
 *        pct_before` AND at least one `delta_pct > 0`.
 *     5. All 4 privacy flags == true.
 *     6. σ_tutor ∈ [0, 1] AND σ_tutor == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v254.1 (named, not implemented): live BKT online-
 *     update loop, v141 curriculum generator running
 *     against a real exercise bank, v113 sandbox hosting
 *     code drills, v220 simulate driving concept
 *     visualisations, v172 live narrative with consented
 *     user opt-in, v182 `cos tutor --export` producing a
 *     signed portable archive.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V254_TUTOR_H
#define COS_V254_TUTOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V254_N_SKILLS     4
#define COS_V254_N_EXERCISES  4
#define COS_V254_N_MODALITIES 4
#define COS_V254_N_PROGRESS   3
#define COS_V254_N_PRIVACY    4

typedef struct {
    char  name[24];
    float p_mastery;
    float sigma_mastery;
    bool  skill_ok;
} cos_v254_skill_t;

typedef struct {
    char  skill[24];
    float difficulty;
    float learner_level;
    float sigma_difficulty;
    bool  fit;
} cos_v254_exercise_t;

typedef struct {
    char  name[16];
    float sigma_fit;
    bool  chosen;
} cos_v254_modality_t;

typedef struct {
    char  skill[24];
    int   pct_before;
    int   pct_after;
    int   delta_pct;
} cos_v254_progress_t;

typedef struct {
    char  flag[24];
    bool  enabled;
} cos_v254_privacy_t;

typedef struct {
    cos_v254_skill_t     skills    [COS_V254_N_SKILLS];
    cos_v254_exercise_t  exercises [COS_V254_N_EXERCISES];
    cos_v254_modality_t  modalities[COS_V254_N_MODALITIES];
    cos_v254_progress_t  progress  [COS_V254_N_PROGRESS];
    cos_v254_privacy_t   privacy   [COS_V254_N_PRIVACY];

    float tau_fit;

    int   n_skills_ok;
    int   n_exercises_fit;
    int   n_chosen_modalities;
    int   n_positive_progress;
    int   n_privacy_enabled;

    float sigma_tutor;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v254_state_t;

void   cos_v254_init(cos_v254_state_t *s, uint64_t seed);
void   cos_v254_run (cos_v254_state_t *s);

size_t cos_v254_to_json(const cos_v254_state_t *s,
                         char *buf, size_t cap);

int    cos_v254_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V254_TUTOR_H */

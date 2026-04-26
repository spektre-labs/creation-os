/*
 * Predictive σ-world (lab): coarse domain + prompt-shape features to
 * estimate σ before (or alongside) full inference.  Distinct from
 * src/sigma/world_model.c (knowledge-graph beliefs).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_OMEGA_PREDICTIVE_WORLD_H
#define COS_OMEGA_PREDICTIVE_WORLD_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_predictive_world {
    float domain_sigma[16];
    int   domain_query_count[16];
    float complexity_coeffs[4];
    float model_ceiling[8];
    float sigma_trend;
    int   trend_window;
} cos_predictive_world_t;

void cos_predictive_world_init(cos_predictive_world_t *wm);

/** Keyword buckets 0..15 (coarse; not a classifier benchmark). */
int cos_predictive_classify_domain(const char *prompt);

int cos_predictive_count_entities(const char *prompt);
int cos_predictive_count_negations(const char *prompt);

/** Bounded [0,1] pre-call σ estimate from current running stats. */
float cos_predictive_sigma_precall(const cos_predictive_world_t *wm,
                                   const char *prompt,
                                   const char *model_id);

void cos_predictive_world_record_observation(cos_predictive_world_t *wm,
                                             int domain, float observed_sigma);

cos_predictive_world_t *cos_predictive_world_singleton(void);

/** Ω-loop hook: update singleton from one completed turn. */
void cos_predictive_world_omega_note_turn(const char *goal, float observed_sigma,
                                          int turn_count_after_step);

void cos_predictive_world_fprint_report(FILE *fp, const char *prompt_opt,
                                        const char *model_opt);

int cos_predictive_world_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_OMEGA_PREDICTIVE_WORLD_H */

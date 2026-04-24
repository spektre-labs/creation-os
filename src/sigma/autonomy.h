/*
 * Autonomous mode brain — σ-driven transitions on top of Ω-loop turns.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_AUTONOMY_H
#define COS_SIGMA_AUTONOMY_H

#include "curiosity.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_omega_state;

enum {
    COS_AUTONOMY_IDLE       = 0,
    COS_AUTONOMY_LEARNING   = 1,
    COS_AUTONOMY_PRACTICING = 2,
    COS_AUTONOMY_EXPLORING  = 3,
    COS_AUTONOMY_SERVING    = 4,
};

struct cos_autonomy_state {
    int                     mode;
    int                     prev_mode;
    struct cos_curiosity_target current_target;
    float                   productivity;
    float                   growth_rate;
    int                     goals_completed;
    int                     goals_failed;
    int                     self_generated_goals;
    int64_t                 autonomous_since_ms;
    float                   last_learn_sigma_before;
    float                   last_learn_sigma_after;
    int                     phase_ticks;
    char                    last_reason[160];
    char                    next_hint[1024];
    int                     inited;
};

void cos_autonomy_configure(int enable_omega_autonomy);

int cos_autonomy_init(struct cos_autonomy_state *as);

int cos_autonomy_decide(struct cos_autonomy_state *as, char *next_action,
                        int max_len);

int cos_autonomy_transition(struct cos_autonomy_state *as, int new_mode,
                            const char *reason);

void cos_autonomy_report(const struct cos_autonomy_state *as);

/** Bind state for `cos introspect` (Ω-loop holds the struct). */
void cos_autonomy_bind_for_introspect(struct cos_autonomy_state *as);

void cos_autonomy_note_learn(struct cos_autonomy_state *as, uint64_t domain_hash,
                             float sigma_before, float sigma_after);

void cos_autonomy_introspect_print(FILE *fp);

/**
 * Per-Ω-turn hook: refresh curiosity, nudge modes, optional learn pass.
 */
void cos_autonomy_omega_tick(struct cos_autonomy_state *as,
                             const struct cos_omega_state *omega_st,
                             float sigma_turn, const char *goal_text,
                             const char *output_text);

int cos_autonomy_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_AUTONOMY_H */

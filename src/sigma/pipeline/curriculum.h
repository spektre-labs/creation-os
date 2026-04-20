/*
 * σ-Curriculum — progressive difficulty controller, σ as promotion.
 *
 * S1 generates one round at a time.  σ-Curriculum turns a stream of
 * those rounds into a level progression: start at level 1, require
 * `min_rounds` consecutive verified rounds (σ_a < τ), then promote.
 * A single failure resets the streak but does not demote — demotion
 * is a caller policy choice, not a state-machine invariant.
 *
 * Canonical ladder (pipeline-level taxonomy; the caller picks the
 * prompt templates per level):
 *
 *   Level 1  FACTUAL     "What is X?"
 *   Level 2  REASONING   "Why does X cause Y?"
 *   Level 3  MULTISTEP   "Given A and B, derive C"
 *   Level 4  CREATIVE    "Design a system that…"
 *   Level 5  META        "What don't you know about X?"
 *
 * This primitive is format-agnostic: it tracks state (level,
 * streak, promotions, history), nothing else.  The σ-Selfplay
 * loop, the TTT primitive, the Codex, and the engram layer all
 * treat it as their curriculum clock.
 *
 * Contracts (v0):
 *   1. init clamps `max_level ≥ 1` and `min_rounds ≥ 1`.
 *   2. step(σ) returns NO_CHANGE / PROMOTED / MAXED; never demotes.
 *   3. step with σ_a < τ at level == max_level stamps MAXED and
 *      leaves level == max_level (no promotion past the cap).
 *   4. snapshot() is pure-read; history is ring-buffered at cap.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CURRICULUM_H
#define COS_SIGMA_CURRICULUM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_CUR_NO_CHANGE = 0,
    COS_CUR_RESET     = 1,   /* streak broken                         */
    COS_CUR_PROMOTED  = 2,   /* level advanced this step              */
    COS_CUR_MAXED     = 3,   /* at max_level, succeeded, cannot promote */
} cos_curriculum_event_t;

typedef struct {
    int   level;             /* 1-based                               */
    int   max_level;
    int   min_rounds;        /* streak required to promote            */
    float sigma_threshold;   /* σ_a below this counts as a success    */
    int   current_streak;
    int   total_rounds;
    int   total_success;
    int   total_promotions;
    int   last_promotion_at; /* total_rounds when last promotion hit  */
    float streak_sigma_sum;  /* running sum of successful σ_a         */
} cos_curriculum_state_t;

void cos_sigma_curriculum_init(cos_curriculum_state_t *s,
                               int max_level,
                               int min_rounds,
                               float sigma_threshold);

cos_curriculum_event_t cos_sigma_curriculum_step(
    cos_curriculum_state_t *s, float sigma_answer);

/* Canonical human-readable label for a level.  Returns a static
 * string, never NULL.  Out-of-range levels map to "UNKNOWN". */
const char *cos_sigma_curriculum_level_name(int level);

/* Acceptance rate across the whole run = success / rounds. */
float cos_sigma_curriculum_acceptance_rate(const cos_curriculum_state_t *s);

int cos_sigma_curriculum_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_CURRICULUM_H */

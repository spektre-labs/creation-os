/*
 * v172 σ-Narrative — long-horizon contextual understanding
 * across sessions: session summaries, chained narrative,
 * goal tracking, people memory, resumption protocol.
 *
 * v115 holds short-term memory *within* a session.  v172
 * stitches sessions into a durable story: who / what / why /
 * where-we-left-off, each piece σ-scored so low-confidence
 * recalls are flagged instead of hallucinated.
 *
 * v172.0 (this file) ships:
 *   - a baked fixture of 3 sessions, each with 4 facts;
 *     facts mention goals ("Creation OS v1.0 release"),
 *     kernel-version progress ("done: v84..v111") and
 *     people ("lauri", "rene", "mikko", "topias") with
 *     a synthetic role and last-seen timestamp
 *   - σ per session summary (coverage — how much of the
 *     session is captured)
 *   - a narrative thread = concatenated summaries, chained
 *     by index, producing a single coherent story
 *   - goal tracking: σ per goal reflects progress confidence
 *   - relationship memory: per-person {role, last_mention,
 *     last_context, σ_ident}
 *   - resumption protocol: given the narrative, produces the
 *     deterministic opener for a new session
 *
 * v172.1 (named, not shipped):
 *   - real v115 memory iteration
 *   - BitNet-generated summaries
 *   - incremental chained narrative (new session extends,
 *     never rewrites)
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V172_NARRATIVE_H
#define COS_V172_NARRATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V172_MAX_SESSIONS   8
#define COS_V172_FACTS_PER_SESS 8
#define COS_V172_MAX_GOALS      6
#define COS_V172_MAX_PEOPLE     8
#define COS_V172_MAX_STR       160
#define COS_V172_MAX_RESUME    320

typedef struct {
    int   id;                             /* 0..n-1 */
    char  label   [COS_V172_MAX_STR];     /* "week 1" */
    char  summary [COS_V172_MAX_STR];
    int   n_facts;
    char  facts[COS_V172_FACTS_PER_SESS][COS_V172_MAX_STR];
    float sigma_coverage;                 /* ∈ [0,1] — lower = better */
} cos_v172_session_t;

typedef struct {
    char  name     [COS_V172_MAX_STR];
    char  progress [COS_V172_MAX_STR];    /* e.g. "done: v84..v163" */
    float sigma_progress;                 /* confidence in that progress */
    bool  done;
} cos_v172_goal_t;

typedef struct {
    char  name        [COS_V172_MAX_STR / 2];
    char  role        [COS_V172_MAX_STR / 2];
    int   last_session_id;
    char  last_context[COS_V172_MAX_STR];
    float sigma_ident;                    /* identification confidence */
} cos_v172_person_t;

typedef struct {
    int                n_sessions;
    cos_v172_session_t sessions[COS_V172_MAX_SESSIONS];

    int                n_goals;
    cos_v172_goal_t    goals[COS_V172_MAX_GOALS];

    int                n_people;
    cos_v172_person_t  people[COS_V172_MAX_PEOPLE];

    float              tau_coverage;       /* default 0.30 — above ⇒ "shaky summary" */
    uint64_t           seed;
} cos_v172_state_t;

void cos_v172_init(cos_v172_state_t *s, uint64_t seed);

/* Build the 3-session baked fixture. */
void cos_v172_fixture_3_sessions(cos_v172_state_t *s);

/* Render the chained narrative thread into `buf`. */
size_t cos_v172_narrative_thread(const cos_v172_state_t *s,
                                 char *buf, size_t cap);

/* Render the deterministic resumption opener. */
size_t cos_v172_resume(const cos_v172_state_t *s,
                       char *buf, size_t cap);

const cos_v172_person_t *
       cos_v172_person_get(const cos_v172_state_t *s, const char *name);

size_t cos_v172_to_json(const cos_v172_state_t *s, char *buf, size_t cap);

int cos_v172_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V172_NARRATIVE_H */

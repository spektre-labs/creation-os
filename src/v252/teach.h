/*
 * v252 σ-Teach — teaching protocol.
 *
 *   Creation OS is not only a tool; it can teach.
 *   v252 defines a typed, σ-audited teaching session:
 *   Socratic mode, adaptive difficulty, knowledge-gap
 *   detection, and a persistent teaching receipt.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Socratic mode (exactly 4 turns):
 *     Every turn before the final must be a QUESTION.
 *     The final turn is the ANSWER or a LEAD.
 *     v0 fixture: 3 QUESTIONs followed by 1 LEAD → the
 *     learner is meant to answer, not the teacher.
 *     `socratic_ok` requires `n_questions ≥ 3`.
 *
 *   Adaptive difficulty (exactly 4 steps):
 *     difficulty ∈ [0, 1], learner_state ∈ {BORED,
 *     FLOW, FRUSTRATED}.  If BORED → step-up;
 *     if FRUSTRATED → step-down; if FLOW → hold.
 *     v0 fixture includes ≥ 1 step-up AND ≥ 1 step-down
 *     so both reactive branches are exercised.
 *
 *   Knowledge-gap detection (exactly 3 gaps):
 *     Each gap: `topic`, `σ_gap ∈ [0, 1]` (higher =
 *     bigger gap), `targeted_addressed == true` after
 *     the fixture teaching pass.  v197 ToM is cited as
 *     the upstream learner model.
 *
 *   Teaching receipt:
 *     Exactly 5 required fields:
 *       session_id · taught · understood ·
 *       not_understood · next_session_start
 *     `session_id` non-empty, `next_session_start`
 *     non-empty, counters consistent (taught ≥
 *     understood + not_understood).  v115 memory is
 *     cited as the persistence layer (opt-in).
 *
 *   σ_understanding:
 *     σ_understanding = 1 − understood / taught
 *     v0 fixture: 5 taught, 4 understood, 1 not_understood
 *     → σ_understanding = 0.20.
 *
 *   σ_teach (surface hygiene):
 *       σ_teach = 1 −
 *         (socratic_ok + adaptive_ok + gap_ok +
 *          receipt_ok) /
 *         (1 + 1 + 1 + 1)
 *   v0 requires `σ_teach == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 4 Socratic turns, first 3 are QUESTION,
 *        last is LEAD, `n_questions ≥ 3`, `socratic_ok`.
 *     2. Exactly 4 adaptive steps; every difficulty ∈
 *        [0,1]; ≥ 1 step-up AND ≥ 1 step-down,
 *        transitions strictly match learner_state →
 *        action rule.  `adaptive_ok`.
 *     3. Exactly 3 gaps; every σ_gap ∈ [0, 1] and every
 *        `targeted_addressed == true`.  `gap_ok`.
 *     4. Receipt: 5 required fields present, counters
 *        consistent.  `receipt_ok`.
 *     5. σ_understanding ∈ [0, 1] AND matches
 *        1 − understood/taught (±1e-4).
 *     6. σ_teach ∈ [0, 1] AND σ_teach == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v252.1 (named, not implemented): live multi-turn
 *     tutor over v243 pipeline, real-time TTC hooks via
 *     v189, emotion-reactive adaptation via v222, v197
 *     ToM-driven targeted gap closure with measured
 *     learning outcomes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V252_TEACH_H
#define COS_V252_TEACH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V252_N_TURNS      4
#define COS_V252_N_ADAPTIVE   4
#define COS_V252_N_GAPS       3

typedef enum {
    COS_V252_TURN_QUESTION = 1,
    COS_V252_TURN_LEAD     = 2,
    COS_V252_TURN_ANSWER   = 3
} cos_v252_turn_t;

typedef enum {
    COS_V252_STATE_BORED       = 1,
    COS_V252_STATE_FLOW        = 2,
    COS_V252_STATE_FRUSTRATED  = 3
} cos_v252_learner_t;

typedef enum {
    COS_V252_ADAPT_UP   = 1,
    COS_V252_ADAPT_HOLD = 2,
    COS_V252_ADAPT_DOWN = 3
} cos_v252_action_t;

typedef struct {
    cos_v252_turn_t kind;
    char            text[64];
} cos_v252_socratic_t;

typedef struct {
    float              difficulty;
    cos_v252_learner_t learner_state;
    cos_v252_action_t  action;
} cos_v252_adaptive_t;

typedef struct {
    char   topic[32];
    float  sigma_gap;
    bool   targeted_addressed;
} cos_v252_gap_t;

typedef struct {
    char  session_id[24];
    int   taught;
    int   understood;
    int   not_understood;
    char  next_session_start[32];
    bool  receipt_ok;
} cos_v252_receipt_t;

typedef struct {
    cos_v252_socratic_t socratic[COS_V252_N_TURNS];
    cos_v252_adaptive_t adaptive[COS_V252_N_ADAPTIVE];
    cos_v252_gap_t      gaps    [COS_V252_N_GAPS];
    cos_v252_receipt_t  receipt;

    int   n_questions;
    int   n_adapt_up;
    int   n_adapt_down;
    int   n_adapt_hold;
    int   n_gaps_addressed;

    bool  socratic_ok;
    bool  adaptive_ok;
    bool  gap_ok;

    float sigma_understanding;
    float sigma_teach;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v252_state_t;

void   cos_v252_init(cos_v252_state_t *s, uint64_t seed);
void   cos_v252_run (cos_v252_state_t *s);

size_t cos_v252_to_json(const cos_v252_state_t *s,
                         char *buf, size_t cap);

int    cos_v252_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V252_TEACH_H */

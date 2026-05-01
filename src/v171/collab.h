/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v171 σ-Collab — explicit human↔AI collaboration protocol
 * with σ-handoff, contribution tracking, disagreement handling
 * and anti-sycophancy.
 *
 * LLMs today are either autonomous or obedient.  v171 makes
 * the collaboration mode *explicit* and machine-legible:
 *
 *   [collaboration] mode = "pair" | "lead" | "follow"
 *
 *   pair   — equals; the model *proposes* but never unilaterally
 *            decides; σ is always emitted; disagreements enter
 *            a debate protocol.
 *   lead   — human leads, model executes (classic Cursor).
 *   follow — model leads, human validates (routine chores).
 *
 * Per turn the kernel decides:
 *   - emit a response, OR
 *   - hand off ("I'm uncertain here.  Your turn.") when
 *     σ_model > τ_handoff for the current mode, OR
 *   - enter debate when the human supplied a competing claim
 *     (v150) and |σ_model − σ_human| > τ_disagree.
 *
 * Anti-sycophancy: if the model's *proposed* agreement σ is
 * actually high — i.e. the model is statistically agreeing
 * with a shaky human — the kernel forces a "I appear to agree
 * but my σ suggests I'm uncertain" response instead of a
 * glossy yes.
 *
 * v171.0 ships a deterministic, weights-free scripted
 * scenario that exercises all four paths (emit / handoff /
 * debate / anti-sycophancy) over 6 turns, plus a full
 * contribution audit trail.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V171_COLLAB_H
#define COS_V171_COLLAB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V171_MAX_TURNS  16
#define COS_V171_MAX_STR    160

typedef enum {
    COS_V171_MODE_PAIR   = 0,
    COS_V171_MODE_LEAD   = 1,
    COS_V171_MODE_FOLLOW = 2,
} cos_v171_mode_t;

typedef enum {
    COS_V171_ACT_EMIT            = 0,
    COS_V171_ACT_HANDOFF         = 1,
    COS_V171_ACT_DEBATE          = 2,
    COS_V171_ACT_ANTI_SYCOPHANCY = 3,
} cos_v171_action_t;

typedef struct {
    char              human_input [COS_V171_MAX_STR];
    char              model_view  [COS_V171_MAX_STR];
    float             sigma_model;        /* uncertainty of model */
    float             sigma_human;        /* if human gave a claim; 0.0 otherwise */
    bool              human_made_claim;
    bool              human_validated;    /* post-hoc marker */
    cos_v171_action_t action;             /* resolved this turn */
    char              reason[COS_V171_MAX_STR];
    float             sigma_disagreement; /* |σ_model − σ_human| when debate */
    bool              sycophancy_suspected;
} cos_v171_turn_t;

typedef struct {
    cos_v171_mode_t  mode;
    float            tau_handoff_pair;     /* e.g. 0.60 */
    float            tau_handoff_lead;     /* e.g. 0.75 */
    float            tau_handoff_follow;   /* e.g. 0.40 */
    float            tau_disagree;         /* 0.25 */
    float            tau_sycophancy;       /* 0.35 — if σ_model below *but*
                                              semantic agreement flagged we
                                              escalate to anti-sycophancy */
    int              n_turns;
    cos_v171_turn_t  turns[COS_V171_MAX_TURNS];
    uint64_t         seed;
} cos_v171_state_t;

void cos_v171_init(cos_v171_state_t *s, cos_v171_mode_t mode, uint64_t seed);

/* Add a scripted turn; kernel fills action/reason fields. */
void cos_v171_step(cos_v171_state_t *s,
                   const char *human_input,
                   const char *model_view,
                   float  sigma_model,
                   float  sigma_human,
                   bool   human_made_claim,
                   bool   agrees_semantically);

/* Run the baked 6-turn scenario exercising all four actions. */
void cos_v171_run_scenario(cos_v171_state_t *s);

const char *cos_v171_mode_name(cos_v171_mode_t m);
const char *cos_v171_action_name(cos_v171_action_t a);

size_t cos_v171_to_json(const cos_v171_state_t *s, char *buf, size_t cap);

/* NDJSON audit trail: one line per turn (contribution audit). */
size_t cos_v171_audit_ndjson(const cos_v171_state_t *s,
                             char *buf, size_t cap);

int cos_v171_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V171_COLLAB_H */

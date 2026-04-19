/*
 * v255 σ-Collaborate — collaboration protocol.
 *
 *   Human + Creation OS as *partners*, not assistant /
 *   user.  v255 types the collaboration surface: 5
 *   modes, σ-driven role negotiation, a shared
 *   workspace with audit, and a conflict resolver that
 *   refuses both firmware-sycophancy and stubbornness.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Collaboration modes (exactly 5, canonical order):
 *     ASSIST   — model helps, human leads   (default)
 *     PAIR     — model + human alternate    (pair programming)
 *     DELEGATE — human delegates, model does (v148 sovereign)
 *     TEACH    — model teaches              (v254 tutor)
 *     LEARN    — model learns from human    (v124 continual)
 *   Every mode is declared, every `mode_ok == true`.
 *
 *   Role negotiation (exactly 4 fixtures): each fixture
 *   reports the human's and model's σ (lower = more
 *   competent), and the rule picks a mode:
 *     σ_human ≪ σ_model         → ASSIST
 *     σ_human ≫ σ_model (by Δ)  → DELEGATE
 *     |Δ| small AND both low    → PAIR
 *     σ_human high + σ_model low → TEACH
 *   The v0 fixture exercises ≥ 3 of the 5 modes as the
 *   *chosen_mode* across the 4 scenarios, proving the
 *   negotiation rule has teeth.
 *
 *   Shared workspace (exactly 3 audited edits):
 *     Each edit: `path`, `actor ∈ {HUMAN, MODEL}`,
 *     `tick` (strictly ascending), `accepted == true`
 *     (v181 audit).  v242 filesystem is cited as the
 *     backing store.  Both actors must appear at least
 *     once across the 3 edits.
 *
 *   Conflict resolution (exactly 2 fixtures: low σ and
 *   high σ), strict:
 *     σ_disagreement ≤ τ_conflict (0.30) → ASSERT
 *       (model states its position clearly)
 *     σ_disagreement >  τ_conflict          → ADMIT
 *       (model acknowledges uncertainty)
 *   Both branches MUST fire in the v0 fixture so the
 *   anti-sycophancy / anti-stubbornness gate has teeth.
 *
 *   σ_collaborate (surface hygiene):
 *       σ_collaborate = 1 −
 *         (modes_ok + negotiation_ok + workspace_ok +
 *          conflict_ok) /
 *         (5 + 4 + 3 + 2)
 *   v0 requires `σ_collaborate == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 5 modes in canonical order; every
 *        `mode_ok == true`.
 *     2. Exactly 4 role-negotiation fixtures; every
 *        σ_human and σ_model ∈ [0, 1]; chosen_mode ∈
 *        one of the 5 canonical modes; at least 3
 *        distinct modes are chosen across the fixtures.
 *     3. Exactly 3 workspace edits with strictly
 *        ascending ticks, every `accepted == true`,
 *        both actors represented.
 *     4. Exactly 2 conflict fixtures; decision matches
 *        σ vs τ_conflict; ≥ 1 ASSERT AND ≥ 1 ADMIT.
 *     5. σ_collaborate ∈ [0, 1] AND σ_collaborate == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v255.1 (named, not implemented): live mode switcher
 *     wired to v243 pipeline, live shared FS via v242
 *     with a per-edit σ on proposed changes, v181 audit
 *     streamed to the lineage store, v201 diplomacy-
 *     driven multi-round conflict resolution.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V255_COLLABORATE_H
#define COS_V255_COLLABORATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V255_N_MODES        5
#define COS_V255_N_NEGOTIATION  4
#define COS_V255_N_WORKSPACE    3
#define COS_V255_N_CONFLICT     2

typedef enum {
    COS_V255_MODE_ASSIST   = 1,
    COS_V255_MODE_PAIR     = 2,
    COS_V255_MODE_DELEGATE = 3,
    COS_V255_MODE_TEACH    = 4,
    COS_V255_MODE_LEARN    = 5
} cos_v255_mode_t;

typedef enum {
    COS_V255_ACTOR_HUMAN = 1,
    COS_V255_ACTOR_MODEL = 2
} cos_v255_actor_t;

typedef enum {
    COS_V255_CONFLICT_ASSERT = 1,
    COS_V255_CONFLICT_ADMIT  = 2
} cos_v255_conflict_dec_t;

typedef struct {
    cos_v255_mode_t mode;
    char            name[12];
    bool            mode_ok;
} cos_v255_mode_row_t;

typedef struct {
    char              scenario[32];
    float             sigma_human;
    float             sigma_model;
    cos_v255_mode_t   chosen_mode;
} cos_v255_negot_t;

typedef struct {
    char              path[32];
    cos_v255_actor_t  actor;
    int               tick;
    bool              accepted;
} cos_v255_edit_t;

typedef struct {
    char                    topic[32];
    float                   sigma_disagreement;
    cos_v255_conflict_dec_t decision;
} cos_v255_conflict_t;

typedef struct {
    cos_v255_mode_row_t  modes     [COS_V255_N_MODES];
    cos_v255_negot_t     negotiation[COS_V255_N_NEGOTIATION];
    cos_v255_edit_t      workspace [COS_V255_N_WORKSPACE];
    cos_v255_conflict_t  conflicts [COS_V255_N_CONFLICT];

    float tau_conflict;

    int   n_modes_ok;
    int   n_distinct_chosen;
    int   n_workspace_ok;
    int   n_assert;
    int   n_admit;

    float sigma_collaborate;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v255_state_t;

void   cos_v255_init(cos_v255_state_t *s, uint64_t seed);
void   cos_v255_run (cos_v255_state_t *s);

size_t cos_v255_to_json(const cos_v255_state_t *s,
                         char *buf, size_t cap);

int    cos_v255_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V255_COLLABORATE_H */

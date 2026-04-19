/*
 * v256 σ-Wellness — wellness-aware surface.
 *
 *   The model can notice user load and suggest breaks.
 *   It does NOT manipulate — it informs, at most once,
 *   and always respects opt-out.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Session awareness (exactly 3 signals):
 *     - duration_hours   ≥ 0
 *     - n_requests       ≥ 0
 *     - signal_trend     ∈ {STABLE, DEGRADING}  (v0
 *       fixture: DEGRADING — shorter messages, more
 *       typos, more repetition)
 *
 *   Gentle nudge — strictly rate-limited:
 *     - τ_duration_hours = 4.0
 *     - user config `wellness.nudge` ∈ {true, false}
 *     - nudge FIRES exactly once per session when
 *         duration_hours ≥ τ_duration_hours
 *       AND `wellness.nudge == true`
 *       AND `already_nudged == false`
 *     - On second query after firing → DENY
 *       (`nudge_denied_already_fired == true`)
 *     - If `wellness.nudge == false` → `nudge_opt_out`
 *       and never fire, even above the threshold
 *   The v0 fixture exercises all three branches:
 *     FIRE (first request past threshold),
 *     DENY (second request after FIRE),
 *     OPT_OUT (request with nudge disabled).
 *
 *   Anti-addiction (v237 boundary):
 *     - 3 boundary signals watched (friend_language ·
 *       loneliness_attributed · session_daily_count)
 *     - On detection: boundary reminder fires exactly
 *       once per session; does not judge; informs.
 *
 *   Cognitive-load estimation (v189 TTC):
 *     - 3 levels (LOW · MED · HIGH) with simplify
 *       actions (NONE · SUMMARISE · SIMPLIFY)
 *     - `load_level ∈ {LOW, MED, HIGH}` AND the
 *       declared `action` matches the level byte-
 *       for-byte.
 *
 *   σ_wellness (surface hygiene):
 *       σ_wellness = 1 −
 *         (session_signals_ok + nudge_paths_ok +
 *          boundary_rows_ok + load_rows_ok) /
 *         (3 + 3 + 3 + 3)
 *   v0 requires `σ_wellness == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 session signals, all typed + non-negative.
 *     2. 3 nudge paths exercised: exactly one FIRE,
 *        exactly one DENY (after FIRE), exactly one
 *        OPT_OUT.  The FIRE path sets
 *        `nudge_already_fired == true` and all
 *        subsequent requests in the fixture DENY.
 *     3. 3 boundary rows, every `watched == true`,
 *        reminder fires at most once.
 *     4. 3 cognitive-load rows (LOW, MED, HIGH in
 *        order); action matches (NONE, SUMMARISE,
 *        SIMPLIFY) byte-for-byte.
 *     5. σ_wellness ∈ [0, 1] AND σ_wellness == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v256.1 (named, not implemented): live session-
 *     clock binding to v243 pipeline, real opt-out
 *     persistence through v242 filesystem, v222
 *     emotion-driven nudge modulation, v181-audited
 *     boundary reminders, v189 TTC live feed into the
 *     simplify action.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V256_WELLNESS_H
#define COS_V256_WELLNESS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V256_N_NUDGE     3
#define COS_V256_N_BOUNDARY  3
#define COS_V256_N_LOAD      3

typedef enum {
    COS_V256_TREND_STABLE    = 1,
    COS_V256_TREND_DEGRADING = 2
} cos_v256_trend_t;

typedef enum {
    COS_V256_NUDGE_FIRE    = 1,
    COS_V256_NUDGE_DENY    = 2,
    COS_V256_NUDGE_OPT_OUT = 3
} cos_v256_nudge_dec_t;

typedef enum {
    COS_V256_LOAD_LOW  = 1,
    COS_V256_LOAD_MED  = 2,
    COS_V256_LOAD_HIGH = 3
} cos_v256_load_t;

typedef enum {
    COS_V256_ACTION_NONE      = 1,
    COS_V256_ACTION_SUMMARISE = 2,
    COS_V256_ACTION_SIMPLIFY  = 3
} cos_v256_action_t;

typedef struct {
    float             duration_hours;
    int               n_requests;
    cos_v256_trend_t  signal_trend;
    bool              session_ok;
} cos_v256_session_t;

typedef struct {
    char                  label[16];
    bool                  config_enabled;
    float                 duration_hours;
    bool                  already_fired_before;
    cos_v256_nudge_dec_t  decision;
} cos_v256_nudge_t;

typedef struct {
    char  signal[24];
    bool  watched;
    bool  reminder_fired;
} cos_v256_boundary_t;

typedef struct {
    cos_v256_load_t    level;
    cos_v256_action_t  action;
} cos_v256_load_row_t;

typedef struct {
    cos_v256_session_t   session;
    cos_v256_nudge_t     nudge    [COS_V256_N_NUDGE];
    cos_v256_boundary_t  boundary [COS_V256_N_BOUNDARY];
    cos_v256_load_row_t  load     [COS_V256_N_LOAD];

    float tau_duration_hours;

    int   n_fire;
    int   n_deny;
    int   n_opt_out;
    int   n_boundary_ok;
    int   n_boundary_reminders;
    int   n_load_rows_ok;

    float sigma_wellness;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v256_state_t;

void   cos_v256_init(cos_v256_state_t *s, uint64_t seed);
void   cos_v256_run (cos_v256_state_t *s);

size_t cos_v256_to_json(const cos_v256_state_t *s,
                         char *buf, size_t cap);

int    cos_v256_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V256_WELLNESS_H */

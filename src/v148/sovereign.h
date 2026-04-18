/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v148 σ-Sovereign — autonomous self-improvement orchestrator.
 *
 * v148 is the capstone of the v144–v147 world-intelligence
 * stack: the loop that composes every other kernel into one
 * operational cycle.  Each iteration runs six ordered stages:
 *
 *     1. MEASURE   — v143 benchmark (scalar score for v144)
 *     2. IDENTIFY  — v141 curriculum weakness + v133 meta health
 *     3. IMPROVE   — v145 skill acquire on the weakest topic
 *     4. EVOLVE    — v146 genesis proposes a new kernel skeleton
 *     5. REFLECT   — v147 on a canonical reasoning trace
 *     6. SHARE     — v145 share-gate sweep at τ
 *
 * Two σ-gates pace the loop:
 *
 *     G1  stability: if σ_rsi(v144) > τ_sovereign, the loop
 *         self-pauses ("system unstable, manual review") — the
 *         user spec's "σ_rsi > τ_sovereign ⇒ log + notify + wait"
 *         clause, implemented as paused_for_review = 1 plus a
 *         machine-readable reason.
 *
 *     G2  supervision: in SUPERVISED mode every structural change
 *         (skill install, new kernel) is recorded but not
 *         applied; pending_approvals > 0 until the user calls
 *         cos_v148_approve_all() or the matching
 *         cos_v14N_approve() per-kernel routines.  In AUTONOMOUS
 *         mode the σ-gates on each sub-kernel do the gating and
 *         v148 simply records the outcome.
 *
 * Emergency stop (cos stop-sovereign): sets emergency_stopped=1;
 * subsequent cycle() calls short-circuit without touching state.
 * A sovereign loop that has been emergency-stopped can only be
 * restarted by explicit cos_v148_resume_from_emergency() — the
 * resume() alias used by RSI is *not* sufficient.
 */
#ifndef COS_V148_SOVEREIGN_H
#define COS_V148_SOVEREIGN_H

#include <stddef.h>
#include <stdint.h>

#include "../v144/rsi.h"
#include "../v145/skill.h"
#include "../v146/genesis.h"
#include "../v147/reflect.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_V148_SUPERVISED = 0,
    COS_V148_AUTONOMOUS = 1
} cos_v148_mode_t;

typedef struct cos_v148_config {
    cos_v148_mode_t mode;
    float           tau_sovereign;     /* σ_rsi pause threshold      */
    float           tau_skill_install; /* v145 install σ-gate        */
    float           tau_skill_share;   /* v145 share σ-gate          */
    float           tau_genesis_merge; /* v146 σ_code merge-candidate */
    uint64_t        seed;              /* deterministic PRNG key     */
    float           starting_score;    /* baseline for v144          */
} cos_v148_config_t;

/* One line of the cycle's action log.  Order of fields is the
 * canonical stage order above. */
typedef struct cos_v148_cycle_report {
    int   cycle_index;

    /* MEASURE */
    float measured_score;
    int   rsi_accepted;
    int   rsi_rolled_back;
    int   rsi_paused_for_review;     /* from v144 paused             */
    float sigma_rsi;

    /* IDENTIFY */
    int   weakness_reported;         /* always 1 in v148.0 (synthetic) */
    char  weakness_topic[COS_V145_TOPIC_CAP];

    /* IMPROVE */
    int   skill_generated;           /* 1 iff acquire attempted       */
    int   skill_installed;           /* 1 iff σ-gate passed           */
    int   skill_count_after;

    /* EVOLVE */
    int   kernel_generated;          /* 1 iff genesis produced cand   */
    int   kernel_status;             /* cos_v146_status_t numeric     */
    float kernel_sigma_code;

    /* REFLECT */
    int   reflect_weakest_step;
    float reflect_weakest_sigma;
    int   reflect_consistent;

    /* SHARE */
    int   n_shared_after_sweep;

    /* global flags */
    int   mode;                      /* supervised / autonomous        */
    int   pending_approvals;         /* skill + kernel waiting review */
    int   emergency_stopped;
    int   unstable_halt;             /* 1 iff σ_rsi > τ_sovereign      */
} cos_v148_cycle_report_t;

typedef struct cos_v148_state {
    cos_v148_config_t  cfg;

    cos_v144_rsi_t     rsi;
    cos_v145_library_t library;
    cos_v146_state_t   genesis;

    int                cycles_run;
    int                n_actions_total;  /* cumulative stage executions */
    int                emergency_stopped;
    int                unstable_halts;   /* lifetime count              */

    /* Rolling PRNG cursor used for the measured_score trajectory. */
    uint64_t           prng;
} cos_v148_state_t;

/* Initialise with a config (nulls → defaults).  Seeds v144 with
 * cfg.starting_score, and both v145 library and v146 state are
 * left empty (they grow as the loop runs). */
void cos_v148_init(cos_v148_state_t *s, const cos_v148_config_t *cfg);

/* Emit a reasonable default config (SUPERVISED, seed 0x501EC08,
 * starting score 0.50). */
void cos_v148_default_config(cos_v148_config_t *out);

/* Run exactly one sovereign cycle.  Returns 0 on success (even
 * if the cycle self-halts on an unstable or emergency condition);
 * <0 on argument error.  *out is always populated so the caller
 * can stream the JSON into v108 UI. */
int  cos_v148_cycle(cos_v148_state_t *s,
                    cos_v148_cycle_report_t *out);

/* Emergency-stop + resume.  emergency_stop is the hot path that
 * `cos stop-sovereign` will invoke. */
void cos_v148_emergency_stop(cos_v148_state_t *s);
void cos_v148_resume_from_emergency(cos_v148_state_t *s);

/* Approve every pending v146 candidate + mark every v145 skill
 * as installed-and-reviewed.  Used by cos_v148_cycle() in
 * AUTONOMOUS mode; exposed so SUPERVISED operators can call it
 * manually after review. */
int  cos_v148_approve_all(cos_v148_state_t *s);

/* JSON serialisation (cycle report and full state). */
int  cos_v148_cycle_to_json(const cos_v148_cycle_report_t *r,
                            char *buf, size_t cap);
int  cos_v148_state_to_json(const cos_v148_state_t *s,
                            char *buf, size_t cap);

int  cos_v148_self_test(void);

#ifdef __cplusplus
}
#endif
#endif

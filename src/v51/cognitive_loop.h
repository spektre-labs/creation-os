/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v51 cognitive loop — integration scaffold (lab tier).
 *
 * Intent (honest): this is a *wiring shape* that shows how a
 * Perceive → Plan → Execute → Verify → Reflect → Learn loop can be
 * assembled on top of the v33–v50 σ labs. It does **not** run a real
 * transformer forward pass, and the individual phases are pure-C stubs
 * that consume `sigma_decomposed_t` from `src/sigma/decompose.h`.
 *
 * Tier: integration scaffold (C). Not "AGI-complete". See
 * docs/WHAT_IS_REAL.md and docs/v51/ARCHITECTURE.md for scope.
 */
#ifndef CREATION_OS_V51_COGNITIVE_LOOP_H
#define CREATION_OS_V51_COGNITIVE_LOOP_H

#include "../sigma/decompose.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    V51_DIFF_EASY   = 0,
    V51_DIFF_MEDIUM = 1,
    V51_DIFF_HARD   = 2
} v51_difficulty_t;

typedef enum {
    V51_ACTION_ANSWER   = 0, /* σ < lo  → System 1 */
    V51_ACTION_THINK    = 1, /* lo ≤ σ < mid → System 2 (budget forcing) */
    V51_ACTION_DEEP     = 2, /* mid ≤ σ < hi → deep-think / best-of-N */
    V51_ACTION_FALLBACK = 3, /* hi ≤ σ < abstain → escalate to larger model */
    V51_ACTION_ABSTAIN  = 4  /* σ ≥ abstain → refuse to answer */
} v51_action_t;

typedef struct {
    float answer_below;   /* default 0.20 */
    float think_below;    /* default 0.50 */
    float deep_below;     /* default 0.75 */
    float fallback_below; /* default 0.90 */
} v51_thresholds_t;

void v51_default_thresholds(v51_thresholds_t *t);

typedef struct {
    /* input */
    const char *query;

    /* perceive */
    sigma_decomposed_t initial_sigma;
    v51_difficulty_t   perceived_difficulty;

    /* plan */
    v51_action_t planned_action;
    int          think_budget;   /* "tokens" of deliberation, scaffold only */
    int          n_samples;      /* best-of-N fan-out, scaffold only */

    /* execute */
    const char *response;        /* not owned; scaffold returns static strings */
    sigma_decomposed_t final_sigma;

    /* verify */
    int defense_blocked;         /* 1 if a defense layer would reject */

    /* reflect */
    float calibration_gap;       /* |self_report − evidence|, in [0,1] */
    int   self_correction_applied;

    /* learn */
    float self_play_reward;      /* in [0,1] */
    int   experience_logged;
} v51_cognitive_state_t;

/**
 * Run one scaffold cognitive step. Pure-C, deterministic, no I/O.
 * `logits` + `n_logits` stand in for an inference engine's last-token output.
 * If `logits == NULL`, a synthetic uniform distribution is used so callers
 * can exercise the plumbing without a model loaded.
 */
void v51_cognitive_step(const char *query,
                        const float *logits, int n_logits,
                        const v51_thresholds_t *thresholds,
                        v51_cognitive_state_t *out);

/* Helpers exposed for tests and the agent loop. */
v51_difficulty_t v51_classify_difficulty(const sigma_decomposed_t *s);
v51_action_t     v51_decode_action(const sigma_decomposed_t *s,
                                   const v51_thresholds_t *t);
int              v51_compute_think_budget(const sigma_decomposed_t *s);
int              v51_compute_n_samples(const sigma_decomposed_t *s);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V51_COGNITIVE_LOOP_H */

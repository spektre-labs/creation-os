/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-TAOR loop — harness scaffold.
 *
 * Design note: this is NOT a Claude Code clone. It is a structural
 * critique expressed in C. Where Claude Code's nO loop runs
 * `while (tool_call) → execute → observe → repeat` and relies on the
 * model to know when to stop, this loop runs
 *
 *   while (tool_call && σ < threshold && making_progress) { … }
 *
 * and the runtime — not the model — decides when to abstain.
 *
 * Tier (honest): integration scaffold (C). There is no transformer
 * embedded here; the loop consumes v51's cognitive_step + a caller-
 * supplied per-iteration logits stream. See docs/v53/ARCHITECTURE.md.
 */
#ifndef CREATION_OS_V53_HARNESS_LOOP_H
#define CREATION_OS_V53_HARNESS_LOOP_H

#include "../../sigma/decompose.h"
#include "../../v51/cognitive_loop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    V53_LOOP_CONTINUE          = 0,
    V53_LOOP_COMPLETE          = 1, /* goal verified */
    V53_LOOP_ABSTAIN_SIGMA     = 2, /* σ above threshold this iter */
    V53_LOOP_ABSTAIN_DRIFT     = 3, /* cumulative σ EWMA above drift cap */
    V53_LOOP_ABSTAIN_NO_PROG   = 4, /* no progress for N iterations */
    V53_LOOP_ABSTAIN_NO_TOOLS  = 5, /* model stopped without a plan */
    V53_LOOP_BUDGET_EXHAUSTED  = 6, /* max_iterations reached */
    V53_LOOP_SAFETY_BLOCK      = 7  /* reserved for defense-layer veto */
} v53_loop_outcome_t;

typedef struct {
    /* configuration (owned by caller) */
    const char *goal;
    int   max_iterations;
    float sigma_threshold;   /* per-iter σ ceiling;  default 0.75 */
    float drift_cap;          /* EWMA cumulative σ cap; default 0.60 */
    int   no_progress_cap;    /* iters with zero progress before abstain */

    /* runtime state */
    int   iteration;
    float cumulative_sigma;   /* EWMA of final σ across iterations */
    int   no_progress_streak;
    int   tool_calls_proposed;
    int   tool_calls_executed;
    int   tool_calls_denied;
    float last_progress;      /* in [0,1]; monotone should rise toward 1.0 */
} v53_harness_state_t;

void v53_harness_init(v53_harness_state_t *h,
                      const char *goal,
                      int max_iterations,
                      float sigma_threshold);

/**
 * Run one step of the σ-TAOR loop.
 *
 * Caller supplies the "model output" via `logits` + `n_logits`; the loop
 * feeds them to v51_cognitive_step, then updates EWMA / progress /
 * outcome. `progress_signal` ∈ [0,1] is the caller's goal-progress
 * estimator (the scaffold does not impose a scoring function).
 *
 * Returns V53_LOOP_CONTINUE when the loop should keep going; any other
 * value is terminal.
 */
v53_loop_outcome_t v53_taor_step(v53_harness_state_t *h,
                                 const float *logits, int n_logits,
                                 float progress_signal,
                                 int tool_calls_parsed);

/* Human-readable outcome for logs / tests. */
const char *v53_outcome_str(v53_loop_outcome_t o);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V53_HARNESS_LOOP_H */

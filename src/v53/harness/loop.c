/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v53 σ-TAOR loop — scaffold.
 */
#include "loop.h"

#include <math.h>
#include <string.h>

void v53_harness_init(v53_harness_state_t *h,
                      const char *goal,
                      int max_iterations,
                      float sigma_threshold)
{
    if (!h) {
        return;
    }
    memset(h, 0, sizeof(*h));
    h->goal             = goal;
    h->max_iterations   = (max_iterations > 0) ? max_iterations : 50;
    h->sigma_threshold  = (sigma_threshold > 0.0f && sigma_threshold <= 1.0f)
                          ? sigma_threshold
                          : 0.75f;
    h->drift_cap        = 0.60f;
    h->no_progress_cap  = 5;
}

static float v53_clip01(float x)
{
    if (!(x >= 0.0f)) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

const char *v53_outcome_str(v53_loop_outcome_t o)
{
    switch (o) {
    case V53_LOOP_CONTINUE:         return "continue";
    case V53_LOOP_COMPLETE:         return "complete";
    case V53_LOOP_ABSTAIN_SIGMA:    return "abstain(σ>threshold)";
    case V53_LOOP_ABSTAIN_DRIFT:    return "abstain(drift)";
    case V53_LOOP_ABSTAIN_NO_PROG:  return "abstain(no-progress)";
    case V53_LOOP_ABSTAIN_NO_TOOLS: return "abstain(no-tool-calls)";
    case V53_LOOP_BUDGET_EXHAUSTED: return "budget-exhausted";
    case V53_LOOP_SAFETY_BLOCK:     return "safety-block";
    }
    return "unknown";
}

v53_loop_outcome_t v53_taor_step(v53_harness_state_t *h,
                                 const float *logits, int n_logits,
                                 float progress_signal,
                                 int tool_calls_parsed)
{
    if (!h) {
        return V53_LOOP_SAFETY_BLOCK;
    }
    if (h->iteration >= h->max_iterations) {
        return V53_LOOP_BUDGET_EXHAUSTED;
    }

    /* THINK: run the v51 cognitive step to get σ + action */
    v51_thresholds_t t;
    v51_default_thresholds(&t);
    v51_cognitive_state_t s;
    v51_cognitive_step(h->goal, logits, n_logits, &t, &s);

    /* σ-gate at the loop level (not just per-token). */
    const float total = v53_clip01(s.final_sigma.total);
    if (total > h->sigma_threshold) {
        h->iteration++;
        return V53_LOOP_ABSTAIN_SIGMA;
    }

    /* Accumulate EWMA of σ across iterations → drift detection. */
    h->cumulative_sigma = 0.9f * h->cumulative_sigma + 0.1f * total;
    if (h->cumulative_sigma > h->drift_cap) {
        h->iteration++;
        return V53_LOOP_ABSTAIN_DRIFT;
    }

    /* ACT: caller tells us how many tool calls the model proposed. */
    h->tool_calls_proposed += (tool_calls_parsed > 0) ? tool_calls_parsed : 0;

    /* OBSERVE: progress signal in [0,1]. Monotone rise → goal approach. */
    float prog = v53_clip01(progress_signal);
    float delta = prog - h->last_progress;
    if (!(fabsf(delta) > 1e-4f)) {
        h->no_progress_streak++;
    } else {
        h->no_progress_streak = 0;
    }
    h->last_progress = prog;

    /* Completion: tools returned no call AND progress ≈ 1.0 → goal. */
    if (tool_calls_parsed <= 0) {
        if (prog >= 0.999f) {
            h->iteration++;
            return V53_LOOP_COMPLETE;
        }
        h->iteration++;
        return V53_LOOP_ABSTAIN_NO_TOOLS;
    }

    /* No-progress abstention: only after we've taken enough steps to
     * distinguish "early exploration" from "stuck in a loop". */
    if (h->iteration > h->no_progress_cap &&
        h->no_progress_streak >= h->no_progress_cap) {
        h->iteration++;
        return V53_LOOP_ABSTAIN_NO_PROG;
    }

    h->iteration++;
    return V53_LOOP_CONTINUE;
}

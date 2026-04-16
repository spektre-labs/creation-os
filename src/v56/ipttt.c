/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "ipttt.h"
#include <string.h>

static float v56_clamp01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

void v56_ipttt_init(const v56_ipttt_params_t *p, v56_ipttt_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->alpha_current = p ? p->alpha_base : 0.01f;
    /* Fresh controller begins in warm-up: callers must feed at least
     * `min_steps_between` σ observations before the first update can
     * be allowed.  This mirrors the IP-TTT paper's warm-up phase and
     * removes the "insta-update on boot" foot-gun. */
    s->steps_since_last = 0;
}

/* EWMA bands: baseline tracks slow drift (0.99), recent tracks last
 * few steps (0.80).  Caller feeds σ ∈ [0,1] — we clamp to be safe. */
void v56_ipttt_observe_sigma(v56_ipttt_state_t *s, float observed_sigma)
{
    if (!s) return;
    float x = v56_clamp01(observed_sigma);
    /* First-call seeding: both bands equal the observation */
    if (s->sigma_baseline == 0.0f && s->sigma_recent == 0.0f) {
        s->sigma_baseline = x;
        s->sigma_recent   = x;
    } else {
        s->sigma_baseline = 0.99f * s->sigma_baseline + 0.01f * x;
        s->sigma_recent   = 0.80f * s->sigma_recent   + 0.20f * x;
    }
    s->steps_since_last++;
}

void v56_ipttt_decide(const v56_ipttt_params_t *p,
                      v56_ipttt_state_t *s,
                      int bytes_needed,
                      v56_ipttt_decision_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!p || !s || bytes_needed <= 0) {
        out->reason = V56_IPTTT_DENY_INVALID;
        return;
    }

    float drift = s->sigma_recent - s->sigma_baseline;
    out->observed_drift = drift;

    /* Branchless-friendly ordered checks; each path writes a unique
     * reason so the caller can log precisely. */
    if (bytes_needed + s->bytes_used > p->update_budget_bytes) {
        out->reason = V56_IPTTT_DENY_BUDGET;
        return;
    }
    if (s->steps_since_last < p->min_steps_between) {
        out->reason = V56_IPTTT_DENY_COOLDOWN;
        return;
    }
    if (drift < p->sigma_drift_threshold) {
        out->reason = V56_IPTTT_DENY_NO_DRIFT;
        return;
    }

    out->allow = 1;
    out->reason = V56_IPTTT_ALLOW;
    out->proposed_alpha = s->alpha_current;
    out->bytes_reserved = bytes_needed;
    s->last_decision_allow = 1;
}

void v56_ipttt_commit_or_rollback(const v56_ipttt_params_t *p,
                                  v56_ipttt_state_t *s,
                                  int bytes_committed,
                                  float post_update_sigma)
{
    if (!p || !s) return;
    float post = v56_clamp01(post_update_sigma);

    /* Rollback trigger: σ did NOT improve after the update.
     * We compare against sigma_recent at time of decision, which the
     * caller is expected to have frozen — here we approximate by
     * checking post vs baseline (the update's job is to drive recent
     * back toward baseline or below).  If post > baseline + tolerance,
     * rollback. */
    float tolerance = 0.02f;
    int regressed = (post > s->sigma_baseline + tolerance) ? 1 : 0;

    if (s->last_decision_allow && !regressed) {
        s->bytes_used += bytes_committed;
        s->updates_applied++;
        s->steps_since_last = 0;
        /* Clamp to budget to avoid caller overruns. */
        if (s->bytes_used > p->update_budget_bytes) {
            s->bytes_used = p->update_budget_bytes;
        }
    } else if (s->last_decision_allow && regressed) {
        s->updates_rolled_back++;
        s->alpha_current *= p->rollback_shrink;
        if (s->alpha_current < 1e-6f) s->alpha_current = 1e-6f;
        /* bytes_used unchanged: the caller is expected to have
         * discarded the buffer it reserved. */
        s->steps_since_last = 0;
    }
    s->last_decision_allow = 0;
    /* Also refresh the recent band with the post-update observation. */
    v56_ipttt_observe_sigma(s, post);
}

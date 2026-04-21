/* AGI-2 — σ-guided self-play curriculum. */

#include "agi_selfplay.h"

#include "selfplay.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *domain;
    int         idx;
    float       sigma_target;
    float       last_difficulty;
} agi_stub_t;

static int agi_propose(const char *topic, float difficulty, void *ctx,
                       char *out_q, size_t cap_q, float *out_sigma_q) {
    agi_stub_t *s = (agi_stub_t *)ctx;
    s->last_difficulty = difficulty;
    int n = snprintf(out_q, cap_q, "[%s #%d] %s drill",
                     s->domain != NULL ? s->domain : "topic",
                     s->idx, topic != NULL ? topic : "self");
    if (n < 0 || (size_t)n >= cap_q) return -1;
    *out_sigma_q = 0.08f + 0.02f * (float)(s->idx % 3);
    return 0;
}

static int agi_solve(const char *q, void *ctx,
                     char *out_a, size_t cap_a, float *out_sigma_a) {
    agi_stub_t *s = (agi_stub_t *)ctx;
    (void)q;
    /* σ_answer peaks when difficulty drifts away from σ_target. */
    float d = fabsf(s->last_difficulty - s->sigma_target);
    float sig = 0.22f + 1.40f * d;
    if (sig > 0.96f) sig = 0.96f;
    int n = snprintf(out_a, cap_a, "answer-%d", s->idx);
    if (n < 0 || (size_t)n >= cap_a) return -1;
    *out_sigma_a = sig;
    return 0;
}

int cos_agi_selfplay_run(const char *domain, int n_rounds,
                         float escalation_threshold,
                         cos_agi_selfplay_report_t *out) {
    if (!out || n_rounds <= 0) return -1;
    memset(out, 0, sizeof *out);
    out->n_rounds = n_rounds;

    float sigma_target = 0.30f;
    float difficulty   = 0.35f;
    agi_stub_t st;
    memset(&st, 0, sizeof st);
    st.domain = domain != NULL ? domain : "general";

    cos_selfplay_config_t cfg;
    double sum_sig = 0.0;

    for (int i = 0; i < n_rounds; ++i) {
        st.idx            = i;
        st.sigma_target   = sigma_target;
        st.last_difficulty = difficulty;
        if (cos_sigma_selfplay_init(&cfg, 0.42f, sigma_target, 0.38f,
                                    agi_propose, &st, agi_solve, &st) != 0)
            return -2;
        cos_selfplay_round_t r;
        if (cos_sigma_selfplay_round(&cfg, domain != NULL ? domain : "general",
                                     difficulty, &r) != 0)
            return -3;
        sum_sig += (double)r.sigma_answer;
        if (r.sigma_answer >= escalation_threshold) out->n_escalations++;

        /* Curriculum on σ_answer relative to the learning band. */
        if (r.sigma_answer < 0.22f) {
            sigma_target += 0.04f;
            if (sigma_target > 0.52f) sigma_target = 0.52f;
        } else if (r.sigma_answer > 0.62f) {
            sigma_target -= 0.05f;
            if (sigma_target < 0.18f) sigma_target = 0.18f;
        }
        difficulty = r.new_difficulty;
    }
    out->sigma_mean        = (float)(sum_sig / (double)n_rounds);
    out->sigma_target_end  = sigma_target;
    out->difficulty_end    = difficulty;
    return 0;
}

int cos_agi_selfplay_self_test(void) {
    cos_agi_selfplay_report_t r;
    if (cos_agi_selfplay_run("math", 20, 0.72f, &r) != 0) return -1;
    if (r.n_rounds != 20) return -1;
    if (!(r.sigma_mean > 0.15f && r.sigma_mean < 0.85f)) return -1;
    return 0;
}

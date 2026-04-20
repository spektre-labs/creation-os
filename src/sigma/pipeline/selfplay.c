/* σ-Selfplay primitive (proposer + solver + σ-verifier + difficulty). */

#include "selfplay.h"

#include <stdio.h>
#include <string.h>

static float clamp01f(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

int cos_sigma_selfplay_init(cos_selfplay_config_t *cfg,
                            float tau_verify,
                            float difficulty_target,
                            float controller_gain,
                            cos_selfplay_propose_fn proposer,
                            void *proposer_ctx,
                            cos_selfplay_solve_fn solver,
                            void *solver_ctx)
{
    if (!cfg || !proposer || !solver) return -1;
    if (!(tau_verify > 0.0f && tau_verify <= 1.0f)) return -2;
    if (!(difficulty_target >= 0.0f && difficulty_target <= 1.0f)) return -3;
    if (!(controller_gain > 0.0f && controller_gain <= 1.0f))  return -4;
    memset(cfg, 0, sizeof *cfg);
    cfg->tau_verify        = tau_verify;
    cfg->difficulty_target = difficulty_target;
    cfg->controller_gain   = controller_gain;
    cfg->band_low          = 0.20f;
    cfg->band_high         = 0.60f;
    cfg->proposer          = proposer;
    cfg->proposer_ctx      = proposer_ctx;
    cfg->solver            = solver;
    cfg->solver_ctx        = solver_ctx;
    return 0;
}

int cos_sigma_selfplay_round(const cos_selfplay_config_t *cfg,
                             const char *topic,
                             float current_difficulty,
                             cos_selfplay_round_t *out)
{
    if (!cfg || !out) return -1;
    if (!(current_difficulty >= 0.0f && current_difficulty <= 1.0f)) return -2;
    memset(out, 0, sizeof *out);
    out->difficulty_in = current_difficulty;

    float sigma_q = 1.0f;
    int rc = cfg->proposer(topic, current_difficulty, cfg->proposer_ctx,
                           out->question, sizeof out->question, &sigma_q);
    if (rc != 0) return -3;
    out->sigma_question = clamp01f(sigma_q);

    float sigma_a = 1.0f;
    rc = cfg->solver(out->question, cfg->solver_ctx,
                     out->answer, sizeof out->answer, &sigma_a);
    if (rc != 0) return -4;
    out->sigma_answer = clamp01f(sigma_a);

    if (out->sigma_answer < cfg->band_low)       out->label = COS_SP_TOO_EASY;
    else if (out->sigma_answer >= cfg->band_high) out->label = COS_SP_TOO_HARD;
    else                                          out->label = COS_SP_LEARNING;
    out->verified = (out->sigma_answer < cfg->tau_verify) ? 1 : 0;

    /* Proportional controller: step difficulty toward the target by
     * an amount proportional to the σ error relative to band center.
     * TOO_EASY  (σ_a low)  → raise difficulty
     * TOO_HARD  (σ_a high) → lower difficulty
     * LEARNING              → leave difficulty alone                */
    float d = current_difficulty;
    if (out->label == COS_SP_TOO_EASY) {
        float err = cfg->band_low - out->sigma_answer;   /* > 0 */
        d = current_difficulty + cfg->controller_gain * err;
    } else if (out->label == COS_SP_TOO_HARD) {
        float err = out->sigma_answer - cfg->band_high;  /* > 0 */
        d = current_difficulty - cfg->controller_gain * err;
    }
    /* Gentle drift toward the caller-supplied target so the
     * controller has a steady state. */
    d = d + 0.10f * cfg->controller_gain * (cfg->difficulty_target - d);
    out->new_difficulty = clamp01f(d);
    return 0;
}

int cos_sigma_selfplay_proconductor(cos_selfplay_round_t *r,
                                    cos_selfplay_solve_fn other_solver,
                                    void *other_ctx,
                                    float margin)
{
    if (!r || !other_solver) return -1;
    char a2[1024];
    float sigma_other = 1.0f;
    int rc = other_solver(r->question, other_ctx, a2, sizeof a2, &sigma_other);
    if (rc != 0) return -2;
    r->sigma_answer_other = clamp01f(sigma_other);
    /* Overconfident iff self σ much lower than other σ
     * (self is "more sure" than the independent solver). */
    if (r->sigma_answer_other - r->sigma_answer > margin) {
        r->overconfident = 1;
    }
    return 0;
}

void cos_sigma_selfplay_stats(const cos_selfplay_round_t *rounds, int n,
                              cos_selfplay_stats_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!rounds || n <= 0) return;
    double sum_q = 0.0, sum_a = 0.0;
    for (int i = 0; i < n; ++i) {
        sum_q += (double)rounds[i].sigma_question;
        sum_a += (double)rounds[i].sigma_answer;
        if (rounds[i].verified)            out->n_verified++;
        if (rounds[i].overconfident)       out->n_overconfident++;
        switch (rounds[i].label) {
            case COS_SP_TOO_EASY: out->n_too_easy++; break;
            case COS_SP_LEARNING: out->n_learning++; break;
            case COS_SP_TOO_HARD: out->n_too_hard++; break;
        }
    }
    out->n_rounds     = n;
    out->mean_sigma_q = (float)(sum_q / (double)n);
    out->mean_sigma_a = (float)(sum_a / (double)n);
}

/* --------------------- self-test -------------------------------- */

typedef struct {
    float sigma_q;
    float sigma_a;
    const char *q_text;
    const char *a_text;
} stub_cfg_t;

static int stub_propose(const char *topic, float diff, void *ctx,
                        char *out_q, size_t cap_q, float *out_sigma_q)
{
    stub_cfg_t *c = (stub_cfg_t *)ctx;
    (void)topic; (void)diff;
    if (c && c->q_text) {
        size_t n = strlen(c->q_text);
        if (n >= cap_q) n = cap_q - 1;
        memcpy(out_q, c->q_text, n); out_q[n] = '\0';
    } else {
        snprintf(out_q, cap_q, "question");
    }
    *out_sigma_q = c ? c->sigma_q : 0.1f;
    return 0;
}

static int stub_solve(const char *q, void *ctx,
                      char *out_a, size_t cap_a, float *out_sigma_a)
{
    stub_cfg_t *c = (stub_cfg_t *)ctx;
    (void)q;
    if (c && c->a_text) {
        size_t n = strlen(c->a_text);
        if (n >= cap_a) n = cap_a - 1;
        memcpy(out_a, c->a_text, n); out_a[n] = '\0';
    } else {
        snprintf(out_a, cap_a, "answer");
    }
    *out_sigma_a = c ? c->sigma_a : 0.5f;
    return 0;
}

static float fabsf2(float x) { return x < 0.0f ? -x : x; }

static int check_init_guards(void) {
    cos_selfplay_config_t cfg;
    stub_cfg_t s = {0};
    if (cos_sigma_selfplay_init(NULL,  0.5f, 0.5f, 0.5f,
                                stub_propose, &s, stub_solve, &s) == 0) return 10;
    if (cos_sigma_selfplay_init(&cfg,  0.0f, 0.5f, 0.5f,
                                stub_propose, &s, stub_solve, &s) == 0) return 11;
    if (cos_sigma_selfplay_init(&cfg,  0.5f, 1.5f, 0.5f,
                                stub_propose, &s, stub_solve, &s) == 0) return 12;
    if (cos_sigma_selfplay_init(&cfg,  0.5f, 0.5f, 0.0f,
                                stub_propose, &s, stub_solve, &s) == 0) return 13;
    if (cos_sigma_selfplay_init(&cfg,  0.5f, 0.5f, 0.5f,
                                NULL,         &s, stub_solve, &s) == 0) return 14;
    if (cos_sigma_selfplay_init(&cfg,  0.5f, 0.5f, 0.5f,
                                stub_propose, &s, NULL,        &s) == 0) return 15;
    return 0;
}

static int check_rounds_labels(void) {
    cos_selfplay_config_t cfg;
    stub_cfg_t easy  = {.sigma_q = 0.10f, .sigma_a = 0.05f,
                        .q_text = "what is 2+2?", .a_text = "4"};
    stub_cfg_t mid   = {.sigma_q = 0.15f, .sigma_a = 0.35f,
                        .q_text = "why does X cause Y?", .a_text = "because"};
    stub_cfg_t hard  = {.sigma_q = 0.20f, .sigma_a = 0.80f,
                        .q_text = "unify quantum gravity", .a_text = "?"};

    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            stub_propose, &easy, stub_solve, &easy);
    cos_selfplay_round_t r;
    if (cos_sigma_selfplay_round(&cfg, "math", 0.40f, &r) != 0) return 20;
    if (r.label != COS_SP_TOO_EASY)               return 21;
    if (!r.verified)                               return 22;
    /* TOO_EASY → difficulty should RISE (but pulled toward target 0.40). */
    if (!(r.new_difficulty > 0.40f))              return 23;

    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            stub_propose, &mid, stub_solve, &mid);
    cos_sigma_selfplay_round(&cfg, "math", 0.40f, &r);
    if (r.label != COS_SP_LEARNING)               return 24;
    if (!r.verified)                              return 25;

    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            stub_propose, &hard, stub_solve, &hard);
    cos_sigma_selfplay_round(&cfg, "math", 0.80f, &r);
    if (r.label != COS_SP_TOO_HARD)               return 26;
    if (r.verified)                               return 27;
    /* TOO_HARD → difficulty should DROP from 0.80. */
    if (!(r.new_difficulty < 0.80f))              return 28;
    return 0;
}

static int check_proconductor(void) {
    cos_selfplay_config_t cfg;
    /* Self solver: σ=0.05 (very confident).
     * Other solver: σ=0.60 (much less confident).  Gap = 0.55 > 0.20
     * margin → overconfident = 1. */
    stub_cfg_t self_s  = {.sigma_q = 0.10f, .sigma_a = 0.05f,
                          .q_text = "q", .a_text = "a-self"};
    stub_cfg_t other_s = {.sigma_q = 0.10f, .sigma_a = 0.60f,
                          .q_text = "q", .a_text = "a-other"};
    cos_sigma_selfplay_init(&cfg, 0.50f, 0.40f, 0.50f,
                            stub_propose, &self_s, stub_solve, &self_s);
    cos_selfplay_round_t r;
    cos_sigma_selfplay_round(&cfg, "topic", 0.40f, &r);
    if (cos_sigma_selfplay_proconductor(&r, stub_solve, &other_s, 0.20f) != 0)
        return 30;
    if (!r.overconfident)                                 return 31;
    if (fabsf2(r.sigma_answer_other - 0.60f) > 1e-5f)     return 32;

    /* Two solvers agree within margin → not overconfident. */
    stub_cfg_t other2 = {.sigma_q = 0.10f, .sigma_a = 0.10f,
                         .q_text = "q", .a_text = "a"};
    cos_sigma_selfplay_round(&cfg, "topic", 0.40f, &r);
    if (cos_sigma_selfplay_proconductor(&r, stub_solve, &other2, 0.20f) != 0)
        return 33;
    if (r.overconfident)                                   return 34;
    return 0;
}

static int check_stats(void) {
    cos_selfplay_round_t rs[4];
    memset(rs, 0, sizeof rs);
    rs[0].sigma_question = 0.10f; rs[0].sigma_answer = 0.10f;
    rs[0].label = COS_SP_TOO_EASY; rs[0].verified = 1;
    rs[1].sigma_question = 0.15f; rs[1].sigma_answer = 0.40f;
    rs[1].label = COS_SP_LEARNING; rs[1].verified = 1;
    rs[2].sigma_question = 0.20f; rs[2].sigma_answer = 0.70f;
    rs[2].label = COS_SP_TOO_HARD; rs[2].verified = 0;
    rs[3].sigma_question = 0.10f; rs[3].sigma_answer = 0.05f;
    rs[3].label = COS_SP_TOO_EASY; rs[3].verified = 1; rs[3].overconfident = 1;
    cos_selfplay_stats_t st;
    cos_sigma_selfplay_stats(rs, 4, &st);
    if (st.n_rounds         != 4)   return 40;
    if (st.n_verified       != 3)   return 41;
    if (st.n_too_easy       != 2)   return 42;
    if (st.n_learning       != 1)   return 43;
    if (st.n_too_hard       != 1)   return 44;
    if (st.n_overconfident  != 1)   return 45;
    if (fabsf2(st.mean_sigma_a - 0.3125f) > 1e-4f)  return 46;
    return 0;
}

int cos_sigma_selfplay_self_test(void) {
    int rc;
    if ((rc = check_init_guards()))    return rc;
    if ((rc = check_rounds_labels()))  return rc;
    if ((rc = check_proconductor()))   return rc;
    if ((rc = check_stats()))          return rc;
    return 0;
}

/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Selfplay self-test binary + canonical proposer/solver demo. */

#include "selfplay.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    float sigma_q;
    float sigma_a;
    const char *q;
    const char *a;
} demo_stub_t;

static int demo_propose(const char *topic, float difficulty, void *ctx,
                        char *out_q, size_t cap_q, float *out_sigma_q) {
    demo_stub_t *s = (demo_stub_t *)ctx;
    (void)topic; (void)difficulty;
    snprintf(out_q, cap_q, "%s", s->q);
    *out_sigma_q = s->sigma_q;
    return 0;
}

static int demo_solve(const char *q, void *ctx,
                      char *out_a, size_t cap_a, float *out_sigma_a) {
    demo_stub_t *s = (demo_stub_t *)ctx;
    (void)q;
    snprintf(out_a, cap_a, "%s", s->a);
    *out_sigma_a = s->sigma_a;
    return 0;
}

static const char *label_name(cos_selfplay_label_t l) {
    switch (l) {
        case COS_SP_TOO_EASY: return "TOO_EASY";
        case COS_SP_LEARNING: return "LEARNING";
        case COS_SP_TOO_HARD: return "TOO_HARD";
        default: return "?";
    }
}

int main(int argc, char **argv) {
    int rc = cos_sigma_selfplay_self_test();

    /* Canonical mini-curriculum: three stubbed rounds walking the
     * difficulty controller through all three bands. */
    demo_stub_t easy = {.sigma_q = 0.10f, .sigma_a = 0.10f,
                        .q = "what is 2+2?", .a = "4"};
    demo_stub_t mid  = {.sigma_q = 0.15f, .sigma_a = 0.40f,
                        .q = "why does gravity exist?", .a = "mass-energy"};
    demo_stub_t hard = {.sigma_q = 0.20f, .sigma_a = 0.80f,
                        .q = "quantum gravity unification", .a = "?"};
    /* Proconductor comparison: self is overconfident (σ_self=0.05)
     * vs other solver (σ_other=0.70). */
    demo_stub_t other = {.sigma_q = 0.10f, .sigma_a = 0.70f,
                         .q = "q", .a = "a2"};

    cos_selfplay_config_t cfg;
    cos_selfplay_round_t r_easy, r_mid, r_hard, r_pro;

    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            demo_propose, &easy, demo_solve, &easy);
    cos_sigma_selfplay_round(&cfg, "math", 0.40f, &r_easy);
    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            demo_propose, &mid, demo_solve, &mid);
    cos_sigma_selfplay_round(&cfg, "math", 0.40f, &r_mid);
    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            demo_propose, &hard, demo_solve, &hard);
    cos_sigma_selfplay_round(&cfg, "math", 0.80f, &r_hard);

    demo_stub_t self_s = {.sigma_q = 0.10f, .sigma_a = 0.05f,
                          .q = "q", .a = "a-self"};
    cos_sigma_selfplay_init(&cfg, 0.40f, 0.40f, 0.50f,
                            demo_propose, &self_s, demo_solve, &self_s);
    cos_sigma_selfplay_round(&cfg, "math", 0.40f, &r_pro);
    cos_sigma_selfplay_proconductor(&r_pro, demo_solve, &other, 0.20f);

    cos_selfplay_round_t rs[3] = {r_easy, r_mid, r_hard};
    cos_selfplay_stats_t st;
    cos_sigma_selfplay_stats(rs, 3, &st);

    printf("{\"kernel\":\"sigma_selfplay\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"tau_verify\":0.40,\"difficulty_target\":0.40,"
             "\"rounds\":["
               "{\"d_in\":%.2f,\"sigma_q\":%.4f,\"sigma_a\":%.4f,"
                "\"label\":\"%s\",\"verified\":%s,\"d_new\":%.4f},"
               "{\"d_in\":%.2f,\"sigma_q\":%.4f,\"sigma_a\":%.4f,"
                "\"label\":\"%s\",\"verified\":%s,\"d_new\":%.4f},"
               "{\"d_in\":%.2f,\"sigma_q\":%.4f,\"sigma_a\":%.4f,"
                "\"label\":\"%s\",\"verified\":%s,\"d_new\":%.4f}"
             "],"
             "\"stats\":{\"n\":%d,\"n_verified\":%d,\"n_too_easy\":%d,"
                        "\"n_learning\":%d,\"n_too_hard\":%d,"
                        "\"mean_sigma_q\":%.4f,\"mean_sigma_a\":%.4f},"
             "\"proconductor\":{\"sigma_self\":%.4f,\"sigma_other\":%.4f,"
                               "\"overconfident\":%s}"
             "},"
           "\"pass\":%s}\n",
           rc,
           (double)r_easy.difficulty_in, (double)r_easy.sigma_question,
           (double)r_easy.sigma_answer, label_name(r_easy.label),
           r_easy.verified ? "true" : "false",
           (double)r_easy.new_difficulty,
           (double)r_mid.difficulty_in,  (double)r_mid.sigma_question,
           (double)r_mid.sigma_answer,  label_name(r_mid.label),
           r_mid.verified ? "true" : "false",
           (double)r_mid.new_difficulty,
           (double)r_hard.difficulty_in, (double)r_hard.sigma_question,
           (double)r_hard.sigma_answer, label_name(r_hard.label),
           r_hard.verified ? "true" : "false",
           (double)r_hard.new_difficulty,
           st.n_rounds, st.n_verified, st.n_too_easy,
           st.n_learning, st.n_too_hard,
           (double)st.mean_sigma_q, (double)st.mean_sigma_a,
           (double)r_pro.sigma_answer, (double)r_pro.sigma_answer_other,
           r_pro.overconfident ? "true" : "false",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Selfplay demo:\n");
        fprintf(stderr, "  easy:     σ_a=%.2f label=%s d'=%.2f\n",
                (double)r_easy.sigma_answer, label_name(r_easy.label),
                (double)r_easy.new_difficulty);
        fprintf(stderr, "  learning: σ_a=%.2f label=%s d'=%.2f\n",
                (double)r_mid.sigma_answer, label_name(r_mid.label),
                (double)r_mid.new_difficulty);
        fprintf(stderr, "  hard:     σ_a=%.2f label=%s d'=%.2f\n",
                (double)r_hard.sigma_answer, label_name(r_hard.label),
                (double)r_hard.new_difficulty);
    }
    return rc;
}

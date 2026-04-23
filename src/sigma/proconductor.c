/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "proconductor.h"

#include "../import/bitnet_sigma.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static float sigma_pair(const char *prompt, const char *text) {
    return cos_bitnet_sigma_for_prompt_and_output(
        prompt, text ? text : "");
}

static int teacher_allowed_bridge(const char *name, const char *only_list) {
    if (!only_list || !only_list[0]) return 1;
    char wrap[320];
    snprintf(wrap, sizeof wrap, ",%s,", only_list);
    char needle[80];
    snprintf(needle, sizeof needle, ",%s,", name);
    return strstr(wrap, needle) != NULL;
}

static float jaccard_words_simple(const char *a, const char *b) {
    if (!a || !b) return 0.f;
    char ba[4096], bb[4096];
    snprintf(ba, sizeof ba, "%s", a);
    snprintf(bb, sizeof bb, "%s", b);
    char *wa[256];
    char *wb[256];
    int na = 0, nb = 0;
    for (char *t = strtok(ba, " \t\n\r"); t && na < 256; t = strtok(NULL, " \t\n\r"))
        wa[na++] = t;
    for (char *t = strtok(bb, " \t\n\r"); t && nb < 256; t = strtok(NULL, " \t\n\r"))
        wb[nb++] = t;
    if (na == 0 || nb == 0) return 0.f;
    int inter = 0;
    for (int i = 0; i < na; i++)
        for (int j = 0; j < nb; j++)
            if (!strcmp(wa[i], wb[j])) {
                inter++;
                break;
            }
    int uni = na + nb - inter;
    return uni > 0 ? (float)inter / (float)uni : 0.f;
}

static float consensus_score(char *texts[8], int n) {
    if (n < 2) return 0.0f;
    float sum = 0.f;
    int pairs = 0;
    for (int i = 0; i < n; i++)
        for (int j = i + 1; j < n; j++) {
            sum += jaccard_words_simple(texts[i], texts[j]);
            pairs++;
        }
    return pairs > 0 ? sum / (float)pairs : 0.f;
}

int cos_proconductor_run(const char *goal, const char *only_teachers,
                         struct cos_proconductor_session *session) {
    if (!goal || !session) return -1;
    memset(session, 0, sizeof *session);
    snprintf(session->goal, sizeof session->goal, "%s", goal);

    if (cos_distill_init() != 0) return -2;

    char text_ring[8][4096];
    char *tp[8];
    int nt = 0;

    float sig_sum = 0.f;
    float sig_best = 1.0f;
    int best_idx = -1;

    int nc = cos_distill_teacher_count();
    for (int i = 0; i < nc && session->n_examples < 8; i++) {
        const struct cos_distill_teacher *t = cos_distill_teacher_get(i);
        if (!t || !t->available) continue;
        if (!teacher_allowed_bridge(t->name, only_teachers)) continue;

        char rsp[8192];
        rsp[0] = '\0';
        if (cos_distill_fetch_teacher_text(t, goal, rsp, sizeof rsp) != 0)
            continue;

        float sg = sigma_pair(goal, rsp);
        struct cos_distill_example *ex = &session->examples[session->n_examples];
        snprintf(ex->prompt, sizeof ex->prompt, "%s", goal);
        snprintf(ex->best_response, sizeof ex->best_response, "%s", rsp);
        snprintf(ex->teacher, sizeof ex->teacher, "%s", t->name);
        ex->sigma_teacher = sg;
        ex->sigma_local   = sigma_pair(goal, "");
        ex->improvement   = ex->sigma_local - sg;
        ex->timestamp     = (int64_t)time(NULL);

        snprintf(text_ring[nt], sizeof text_ring[nt], "%s", rsp);
        tp[nt] = text_ring[nt];
        nt++;

        sig_sum += sg;
        if (sg < sig_best) {
            sig_best = sg;
            best_idx = session->n_examples;
        }
        session->n_examples++;
    }

    session->sigma_best = session->n_examples > 0 ? sig_best : 1.0f;
    session->sigma_consensus =
        session->n_examples > 0
            ? sig_sum / (float)session->n_examples
            : 1.0f;
    float jc           = consensus_score(tp, nt);
    session->consensus = (nt >= 2 && jc >= 0.35f) ? 1 : 0;

    if (best_idx >= 0) {
        snprintf(session->synthesis, sizeof session->synthesis, "%s",
                 session->examples[best_idx].best_response);
    }
    return 0;
}

void cos_proconductor_print_report(
    const struct cos_proconductor_session *session) {
    if (!session) return;
    printf("goal: %s\n", session->goal);
    if (session->n_examples <= 0)
        fputs("consensus_metric=n/a  sigma_best=n/a  sigma_mean=n/a\n",
              stdout);
    else
        printf("consensus_metric=%s  sigma_best=%.4f  sigma_mean=%.4f\n",
               session->consensus ? "high" : "low",
               (double)session->sigma_best,
               (double)session->sigma_consensus);
    for (int i = 0; i < session->n_examples; i++) {
        printf("  [%s] σ=%.4f  excerpt: %.80s...\n",
               session->examples[i].teacher,
               (double)session->examples[i].sigma_teacher,
               session->examples[i].best_response);
    }
    if (session->synthesis[0])
        printf("synthesis (lowest-σ teacher):\n%s\n", session->synthesis);
}

int cos_proconductor_load_history(int max_rows) {
    return cos_distill_fprint_history(stdout, max_rows);
}

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(CREATION_OS_PROCONDUCTOR_TEST)
int cos_proconductor_self_test(void) {
    struct cos_proconductor_session S;
    memset(&S, 0, sizeof S);
    char prev[512];
    prev[0] = '\0';
    const char *oh = getenv("HOME");
    if (oh) snprintf(prev, sizeof prev, "%s", oh);
    if (setenv("HOME", "/tmp", 1) != 0) return -1;
    (void)setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1);
    (void)setenv("COS_BITNET_SERVER_PORT", "65528", 1);
    cos_distill_shutdown();
    if (cos_proconductor_run("unit test conductor 1+1", NULL, &S) != 0) {
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -1;
    }
    cos_proconductor_print_report(&S);
    float j = jaccard_words_simple("alpha beta gamma", "beta gamma delta");
    if (j <= 0.f) {
        if (prev[0]) (void)setenv("HOME", prev, 1);
        return -2;
    }
    if (prev[0]) (void)setenv("HOME", prev, 1);
    return 0;
}
#endif

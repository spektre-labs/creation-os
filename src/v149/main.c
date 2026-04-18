/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v149 σ-Embodied CLI wrapper.
 */
#include "embodied.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --step ACTION [--magnitude M] [--seed N]\n"
        "                 [--bias B] [--noise S] [--safe T]\n"
        "       %s --rollout N [--seed N] [--bias B] [--noise S] [--safe T]\n"
        " ACTION in {right,left,up,down,forward,back}\n",
        p, p, p);
}

static int parse_action(const char *s, cos_v149_action_id_t *out) {
    if (!strcmp(s, "right"))   { *out = COS_V149_MOVE_RIGHT;   return 0; }
    if (!strcmp(s, "left"))    { *out = COS_V149_MOVE_LEFT;    return 0; }
    if (!strcmp(s, "up"))      { *out = COS_V149_MOVE_UP;      return 0; }
    if (!strcmp(s, "down"))    { *out = COS_V149_MOVE_DOWN;    return 0; }
    if (!strcmp(s, "forward")) { *out = COS_V149_MOVE_FORWARD; return 0; }
    if (!strcmp(s, "back"))    { *out = COS_V149_MOVE_BACK;    return 0; }
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) { usage(argv[0]); return 2; }

    if (!strcmp(argv[1], "--self-test")) {
        int rc = cos_v149_self_test();
        printf("{\"kernel\":\"v149\",\"self_test\":%s,\"rc\":%d}\n",
               rc == 0 ? "\"pass\"" : "\"fail\"", rc);
        return rc;
    }

    int do_step = 0, do_rollout = 0;
    cos_v149_action_id_t aid = COS_V149_MOVE_RIGHT;
    float magnitude = 0.05f, bias = 0.0f, noise = 0.0f, safe = 0.5f;
    uint64_t seed = 0x149ULL;
    int steps = 8;

    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (!strcmp(a, "--step") && i + 1 < argc) {
            do_step = 1;
            if (parse_action(argv[++i], &aid) != 0) { usage(argv[0]); return 2; }
        } else if (!strcmp(a, "--rollout") && i + 1 < argc) {
            do_rollout = 1; steps = atoi(argv[++i]);
            if (steps <= 0 || steps > 1024) { usage(argv[0]); return 2; }
        } else if (!strcmp(a, "--magnitude") && i + 1 < argc) magnitude = (float)atof(argv[++i]);
        else if   (!strcmp(a, "--bias")      && i + 1 < argc) bias      = (float)atof(argv[++i]);
        else if   (!strcmp(a, "--noise")     && i + 1 < argc) noise     = (float)atof(argv[++i]);
        else if   (!strcmp(a, "--safe")      && i + 1 < argc) safe      = (float)atof(argv[++i]);
        else if   (!strcmp(a, "--seed")      && i + 1 < argc) seed      = strtoull(argv[++i], NULL, 0);
        else if   (strcmp(a, argv[0]) != 0 && a[0] == '-') {
            /* unknown flag → usage */
            usage(argv[0]); return 2;
        }
    }

    if (!do_step && !do_rollout) { usage(argv[0]); return 2; }

    cos_v149_sim_t sim;
    cos_v149_sim_init(&sim, seed, bias, noise);

    if (do_step) {
        cos_v149_action_t ac = { aid, magnitude };
        cos_v149_step_report_t rep;
        cos_v149_step_gated(&sim, &ac, safe, &rep);
        char buf[1024];
        cos_v149_step_to_json(&rep, buf, sizeof(buf));
        printf("%s\n", buf);
        return 0;
    }

    /* Rollout: cycle through the 6 directions deterministically. */
    cos_v149_action_t *plan = (cos_v149_action_t *)calloc((size_t)steps, sizeof(*plan));
    if (!plan) return 3;
    for (int i = 0; i < steps; ++i) {
        plan[i].id = (cos_v149_action_id_t)(i % COS_V149_ACTIONS);
        plan[i].magnitude = magnitude;
    }
    cos_v149_step_report_t *reports = (cos_v149_step_report_t *)
        calloc((size_t)steps, sizeof(*reports));
    if (!reports) { free(plan); return 3; }

    int exec = cos_v149_rollout(&sim, plan, steps, safe, 0, reports);

    double sum_emb = 0.0, sum_gap = 0.0;
    int n_exec = 0, n_gate = 0;
    for (int i = 0; i < steps; ++i) {
        if (reports[i].executed) {
            sum_emb += reports[i].sigma_embodied;
            sum_gap += reports[i].sigma_gap;
            n_exec++;
        } else n_gate++;
    }
    printf("{\"kernel\":\"v149\",\"mode\":\"rollout\","
           "\"steps\":%d,\"executed\":%d,\"gated\":%d,"
           "\"mean_sigma_embodied\":%.4f,\"mean_sigma_gap\":%.4f,"
           "\"seed\":%llu,\"bias\":%.4f,\"noise\":%.4f,\"safe\":%.4f}\n",
           steps, exec, n_gate,
           n_exec ? sum_emb / n_exec : 0.0,
           n_exec ? sum_gap / n_exec : 0.0,
           (unsigned long long)seed, bias, noise, safe);

    free(plan); free(reports);
    return 0;
}

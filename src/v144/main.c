/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v144 σ-RSI — CLI.
 */
#include "rsi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo [--scores s0,s1,...,sN] [--baseline 0.5]\n"
        "       %s --pause-demo   (3 consecutive regressions)\n",
        a0, a0, a0);
    return 2;
}

static int parse_scores(const char *csv, float *out, int cap) {
    int n = 0;
    const char *p = csv;
    while (*p && n < cap) {
        char *end = NULL;
        double v = strtod(p, &end);
        if (end == p) break;
        out[n++] = (float)v;
        p = end;
        while (*p == ',' || *p == ' ') p++;
    }
    return n;
}

static int demo_scores(float baseline, const float *scores, int n) {
    cos_v144_rsi_t r;
    cos_v144_init(&r, baseline);
    printf("{\"v144_demo\":true,\"baseline\":%.6f,\"cycles\":[",
           (double)baseline);
    for (int i = 0; i < n; ++i) {
        cos_v144_cycle_report_t c;
        cos_v144_submit(&r, scores[i], &c);
        char buf[1024];
        cos_v144_cycle_to_json(&c, &r, buf, sizeof buf);
        printf("%s%s", i ? "," : "", buf);
    }
    char s[2048];
    cos_v144_state_to_json(&r, s, sizeof s);
    printf("],\"final_state\":%s}\n", s);
    return 0;
}

static int demo_pause(void) {
    /* Ten cycles: six improve, then three regressions trigger pause,
     * then one more submit is ignored (skipped_paused). */
    float scores[] = {
        0.55f, 0.60f, 0.65f, 0.70f, 0.72f, 0.73f,
        0.71f, 0.70f, 0.69f,        /* three regressions */
        0.99f                        /* skipped while paused */
    };
    return demo_scores(0.50f,
                       scores,
                       (int)(sizeof(scores)/sizeof(*scores)));
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v144_self_test();
    if (!strcmp(argv[1], "--pause-demo")) return demo_pause();
    if (!strcmp(argv[1], "--demo")) {
        float baseline = 0.50f;
        float scores[64];
        int   n = 0;
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--baseline") && i + 1 < argc) {
                baseline = (float)atof(argv[++i]);
            } else if (!strcmp(argv[i], "--scores") && i + 1 < argc) {
                n = parse_scores(argv[++i], scores, 64);
            }
        }
        if (n == 0) {
            /* Default trajectory: a realistic improving run with
             * one rollback mid-way. */
            float def[] = {
                0.55f, 0.60f, 0.58f, 0.63f, 0.67f, 0.70f, 0.73f
            };
            memcpy(scores, def, sizeof def);
            n = (int)(sizeof(def)/sizeof(*def));
        }
        return demo_scores(baseline, scores, n);
    }
    return usage(argv[0]);
}

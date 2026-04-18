/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v131 σ-Temporal — CLI wrapper.
 */
#include "temporal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v131_temporal --self-test\n"
        "  creation_os_v131_temporal --trend-demo\n"
        "  creation_os_v131_temporal --decay age_sec sigma half_life_sec\n"
        "  creation_os_v131_temporal --deadline baseline slope fraction\n");
    return 2;
}

static int cmd_trend_demo(void) {
    cos_v131_timeline_t tl; cos_v131_timeline_init(&tl, 32);
    uint64_t base = 1700000000ULL;
    for (int i = 0; i < 10; ++i) {
        cos_v131_event_t e = {0};
        e.ts_unix = base + (uint64_t)i * 86400ULL;
        strncpy(e.topic, "math", sizeof e.topic - 1);
        e.sigma_product = 0.80f - (float)i * 0.07f;
        if (e.sigma_product < 0.05f) e.sigma_product = 0.05f;
        snprintf(e.summary, sizeof e.summary, "math session %d", i);
        cos_v131_timeline_append(&tl, &e);
    }
    int nused = 0;
    float slope = cos_v131_sigma_trend(&tl, "math",
                                       base, base + 10 * 86400ULL, &nused);
    char js[256]; cos_v131_trend_to_json("math", slope, nused, js, sizeof js);
    printf("%s\n", js);

    const cos_v131_event_t *hits[32];
    int n = cos_v131_recall_window(&tl, base, base + 7 * 86400ULL,
                                   NULL, hits, 32);
    printf("{\"recall_window_7d_events\":%d}\n", n);
    cos_v131_timeline_free(&tl);
    return 0;
}

static int cmd_decay(long age, float sig, long hl) {
    float w = cos_v131_decay_weight((uint64_t)age, sig,
                                    (uint64_t)hl, COS_V131_DECAY_ALPHA);
    printf("{\"age_sec\":%ld,\"sigma\":%.4f,\"half_life_sec\":%ld,\"weight\":%.6f}\n",
           age, (double)sig, hl, (double)w);
    return 0;
}

static int cmd_deadline(float base, float slope, float frac) {
    cos_v131_deadline_sigma_t ds = { .baseline_sigma = base, .pressure_slope = slope };
    float s = cos_v131_predict_sigma_under_pressure(&ds, frac);
    printf("{\"baseline\":%.4f,\"slope\":%.4f,\"fraction_used\":%.4f,\"sigma_pred\":%.6f}\n",
           (double)base, (double)slope, (double)frac, (double)s);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test"))   return cos_v131_self_test();
    if (!strcmp(argv[1], "--trend-demo"))  return cmd_trend_demo();
    if (!strcmp(argv[1], "--decay")) {
        if (argc < 5) return usage();
        return cmd_decay(atol(argv[2]), (float)atof(argv[3]), atol(argv[4]));
    }
    if (!strcmp(argv[1], "--deadline")) {
        if (argc < 5) return usage();
        return cmd_deadline((float)atof(argv[2]), (float)atof(argv[3]),
                            (float)atof(argv[4]));
    }
    return usage();
}

/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v134 σ-Spike — CLI wrapper.
 */
#include "spike.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v134_spike --self-test\n"
        "  creation_os_v134_spike --stream <val1,val2,...> "
                 "[--delta F] [--window W] [--count K]\n"
        "  creation_os_v134_spike --demo\n");
    return 2;
}

static int cmd_stream(int argc, char **argv) {
    const char *vals = NULL;
    cos_v134_cfg_t cfg; cos_v134_cfg_defaults(&cfg);
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--stream") && i + 1 < argc) vals = argv[++i];
        else if (!strcmp(argv[i], "--delta" ) && i+1<argc) cfg.delta        = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--window") && i+1<argc) cfg.burst_window = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--count" ) && i+1<argc) cfg.burst_count  = atoi(argv[++i]);
    }
    if (!vals) return usage();

    cos_v134_state_t st; cos_v134_init(&st);
    const char *p = vals;
    int idx = 0;
    while (*p) {
        char *end = NULL;
        float s = (float)strtod(p, &end);
        if (end == p) break;
        cos_v134_verdict_t v = cos_v134_feed(&st, &cfg, s);
        const char *name = v == COS_V134_STABLE ? "STABLE"
                         : v == COS_V134_SPIKE  ? "SPIKE"
                         :                        "BURST";
        printf("t=%d σ=%.4f verdict=%s window_spikes=%d\n",
               idx++, (double)s, name, st.ring_sum);
        p = (*end == ',') ? end + 1 : end;
    }
    char js[256];
    cos_v134_to_json(&st, js, sizeof js);
    printf("summary: %s\n", js);
    return 0;
}

static int cmd_demo(void) {
    cos_v134_cfg_t cfg; cos_v134_cfg_defaults(&cfg);
    cos_v134_state_t st; cos_v134_init(&st);
    /* 20 tokens: 14 stable + 6 spikes → demonstrates ~70% stable. */
    float stream[20] = {
        0.30f, 0.30f, 0.31f, 0.30f, 0.30f,
        0.70f, 0.70f, 0.70f, 0.70f, 0.71f,
        0.30f, 0.30f, 0.31f, 0.30f, 0.70f,
        0.30f, 0.70f, 0.30f, 0.30f, 0.30f
    };
    for (int i = 0; i < 20; ++i) cos_v134_feed(&st, &cfg, stream[i]);
    printf("stable_ratio=%.4f (spec target ≥0.70)\n",
           (double)cos_v134_stable_ratio(&st));
    printf("total_tokens=%llu total_spikes=%llu total_bursts=%llu\n",
           (unsigned long long)st.total_tokens,
           (unsigned long long)st.total_spikes,
           (unsigned long long)st.total_bursts);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v134_self_test();
    if (!strcmp(argv[1], "--demo"))      return cmd_demo();
    if (!strcmp(argv[1], "--stream") || (argc >= 3 && !strcmp(argv[2], "--stream")))
        return cmd_stream(argc - 1, argv + 1);
    return usage();
}

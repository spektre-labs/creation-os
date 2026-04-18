/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v133 σ-Meta — CLI wrapper.
 */
#include "meta.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v133_meta --self-test\n"
        "  creation_os_v133_meta --rising-demo      # 8 weekly snapshots, rising σ\n"
        "  creation_os_v133_meta --noisy-demo       # 8 weekly snapshots, high meta-σ\n"
        "  creation_os_v133_meta --diagnose c0,c1,...,c7  # probe per-channel diagnosis\n");
    return 2;
}

static int emit_health(cos_v133_history_t *h) {
    cos_v133_health_t hl;
    cos_v133_health_compose(h, COS_V133_REGRESSION_TAU_DEFAULT,
                            COS_V133_META_TAU_DEFAULT, 0.40f, &hl);
    char js[1024];
    cos_v133_health_to_json(&hl, js, sizeof js);
    printf("%s\n", js);
    return 0;
}

static int cmd_rising(void) {
    cos_v133_history_t h; cos_v133_history_init(&h);
    uint64_t base = 1700000000ULL;
    for (int i = 0; i < 8; ++i) {
        cos_v133_snapshot_t s = {0};
        s.ts_unix = base + (uint64_t)i * 604800ULL;
        s.avg_sigma_product = 0.30f + (float)i * 0.03f;
        for (int c = 0; c < COS_V133_CHANNELS; ++c)
            s.avg_sigma_per_channel[c] = s.avg_sigma_product;
        s.n_interactions = 100;
        cos_v133_history_append(&h, &s);
    }
    return emit_health(&h);
}

static int cmd_noisy(void) {
    cos_v133_history_t h; cos_v133_history_init(&h);
    uint64_t base = 1700000000ULL;
    float noisy[] = { 0.10f, 0.80f, 0.12f, 0.90f, 0.15f, 0.85f, 0.11f, 0.79f };
    for (int i = 0; i < 8; ++i) {
        cos_v133_snapshot_t s = {0};
        s.ts_unix = base + (uint64_t)i * 604800ULL;
        s.avg_sigma_product = noisy[i];
        for (int c = 0; c < COS_V133_CHANNELS; ++c)
            s.avg_sigma_per_channel[c] = noisy[i];
        s.n_interactions = 100;
        cos_v133_history_append(&h, &s);
    }
    return emit_health(&h);
}

static int cmd_diagnose(const char *csv) {
    cos_v133_snapshot_t s = {0};
    int n = 0;
    const char *p = csv;
    while (*p && n < COS_V133_CHANNELS) {
        char *end = NULL;
        s.avg_sigma_per_channel[n++] = strtof(p, &end);
        if (end == p) break;
        p = end;
        while (*p == ',' || *p == ' ') p++;
    }
    cos_v133_cause_t c = cos_v133_diagnose(&s, 0.40f);
    printf("{\"cause\":\"%s\"}\n", cos_v133_cause_label(c));
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test"))   return cos_v133_self_test();
    if (!strcmp(argv[1], "--rising-demo")) return cmd_rising();
    if (!strcmp(argv[1], "--noisy-demo"))  return cmd_noisy();
    if (!strcmp(argv[1], "--diagnose")) {
        if (argc < 3) return usage();
        return cmd_diagnose(argv[2]);
    }
    return usage();
}

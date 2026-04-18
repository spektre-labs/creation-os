/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v124 σ-Continual CLI: self-test + idle-trigger probe + synthetic
 * epoch replay (for the merge-gate smoke).  No MLX, no weights —
 * that's v124.1 per docs/v124/README.md.
 */
#include "continual.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *argv0) {
    fprintf(stderr,
        "usage:\n"
        "  %s --self-test\n"
        "  %s --trigger IDLE_SEC BUFFER_SIZE NEXT_EPOCH\n"
        "      # prints one of: skip | train | smoke\n"
        "  %s --epoch-healthy N\n"
        "      # simulate N epochs with a healthy baseline,\n"
        "        print stats JSON\n"
        "  %s --epoch-catastrophic N\n"
        "      # simulate N epochs with a 4%% drop at epoch 10,\n"
        "        print stats JSON\n",
        argv0, argv0, argv0, argv0);
    return 2;
}

static const char *trigger_name(cos_v124_trigger_t t) {
    switch (t) {
        case COS_V124_TRIGGER_SKIP:  return "skip";
        case COS_V124_TRIGGER_TRAIN: return "train";
        case COS_V124_TRIGGER_SMOKE: return "smoke";
    }
    return "?";
}

static float healthy(uint32_t adapter, int n, void *ctx) {
    (void)adapter; (void)n; (void)ctx;
    return 0.92f;
}

static float catastrophic(uint32_t adapter, int n, void *ctx) {
    (void)n; (void)ctx;
    if (adapter == 10) return 0.88f;
    return 0.92f;
}

static void fill_buffer(cos_v124_buffer_t *buf, int n) {
    cos_v124_interaction_t it;
    for (int i = 0; i < n; ++i) {
        memset(&it, 0, sizeof it);
        it.ts_unix        = 1700000000ULL + (uint64_t)i;
        it.sigma_product  = (i < (n / 2)) ? 0.72f : 0.20f;
        snprintf(it.prompt,   sizeof it.prompt,   "q-%d", i);
        snprintf(it.response, sizeof it.response, "a-%d", i);
        cos_v124_buffer_append(buf, &it);
    }
}

static int simulate_epochs(int n, cos_v124_baseline_fn bfn) {
    if (n <= 0 || n > 200) { fprintf(stderr, "N out of range\n"); return 2; }
    cos_v124_config_t cfg;
    cos_v124_config_defaults(&cfg);

    cos_v124_buffer_t buf;
    cos_v124_stats_t  stats = {
        .last_baseline_accuracy = 0.92f,
        .active_adapter_version = 0,
    };
    float prev_baseline = 0.92f;
    uint32_t prev_adapter = 0;
    for (int epoch = 0; epoch < n; ++epoch) {
        cos_v124_buffer_init(&buf, cfg.buffer_capacity);
        fill_buffer(&buf, 10);
        cos_v124_epoch_t e;
        cos_v124_run_epoch(&cfg, &buf, prev_adapter, prev_baseline,
                           bfn, NULL, &e);
        stats.epochs_completed++;
        if (e.smoke_ran) {
            stats.epochs_with_smoke++;
            stats.last_baseline_accuracy = e.baseline_accuracy_after;
        }
        if (e.rolled_back) stats.epochs_rolled_back++;
        prev_baseline = e.baseline_accuracy_after;
        prev_adapter  = e.active_adapter_version;
        stats.active_adapter_version = e.active_adapter_version;
    }
    char js[512];
    cos_v124_stats_to_json(&stats, js, sizeof js);
    puts(js);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test")) return cos_v124_self_test();
    if (!strcmp(argv[1], "--trigger") && argc == 5) {
        cos_v124_config_t cfg; cos_v124_config_defaults(&cfg);
        puts(trigger_name(cos_v124_trigger_decide(
            &cfg,
            (uint64_t)strtoull(argv[2], NULL, 10),
            atoi(argv[3]), atoi(argv[4]))));
        return 0;
    }
    if (!strcmp(argv[1], "--epoch-healthy") && argc == 3)
        return simulate_epochs(atoi(argv[2]), healthy);
    if (!strcmp(argv[1], "--epoch-catastrophic") && argc == 3)
        return simulate_epochs(atoi(argv[2]), catastrophic);
    return usage(argv[0]);
}

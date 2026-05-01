/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Merge self-test binary + canonical α-sweep demo. */

#include "merge.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *method_name(cos_merge_method_t m) {
    switch (m) {
        case COS_MERGE_LINEAR:     return "LINEAR";
        case COS_MERGE_SLERP:      return "SLERP";
        case COS_MERGE_TIES:       return "TIES";
        case COS_MERGE_TASK_ARITH: return "TASK_ARITH";
        default:                   return "?";
    }
}

/* Oracle: distance-to-target, scale 4.0. */
typedef struct { const float *target; float scale; } dist_ctx_t;

static float oracle_dist(const float *w, int n, void *ctx) {
    dist_ctx_t *d = (dist_ctx_t *)ctx;
    double s = 0.0;
    for (int i = 0; i < n; ++i) {
        double diff = (double)w[i] - (double)d->target[i];
        s += diff * diff;
    }
    float r = (float)(sqrt(s) / (double)d->scale);
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    return r;
}

int main(int argc, char **argv) {
    int rc = cos_sigma_merge_self_test();

    /* Canonical demo: 4-D target = origin, a & b symmetric around it.
     * Linear merge at α=0.5 should be constructive (lands on origin).
     * α-sweep over {0, .25, .5, .75, 1} picks 0.5 (tie broken to
     * middle since all grid points at 0.5 symmetry).                 */
    float target[4] = {0, 0, 0, 0};
    float a[4] = { 1,  1,  1,  1};
    float b[4] = {-1, -1, -1, -1};
    float scratch[4], out_best[4];
    dist_ctx_t ctx = {.target = target, .scale = 4.0f};

    /* Single α=0.5 evaluation. */
    cos_merge_config_t cfg = {.method = COS_MERGE_LINEAR, .alpha = 0.5f};
    cos_merge_result_t r_single;
    cos_sigma_merge_evaluate(a, b, 4, &cfg, oracle_dist, &ctx,
                             scratch, &r_single);

    /* Full sweep. */
    cos_merge_result_t r_sweep;
    cos_sigma_merge_alpha_sweep(a, b, 4, COS_MERGE_LINEAR, NULL, 0,
                                NULL, 0, oracle_dist, &ctx,
                                scratch, out_best, &r_sweep);

    /* Destructive case: b far from target → α=0.5 merge is worse
     * than a alone; sweep picks α=1 (pure-a). */
    float a2[4] = {0, 0, 0, 0};
    float b2[4] = {2, 2, 2, 2};
    cos_merge_result_t r_dest_single;
    cos_merge_config_t cfg_dest = {.method = COS_MERGE_LINEAR, .alpha = 0.5f};
    cos_sigma_merge_evaluate(a2, b2, 4, &cfg_dest, oracle_dist, &ctx,
                             scratch, &r_dest_single);
    cos_merge_result_t r_dest_sweep;
    cos_sigma_merge_alpha_sweep(a2, b2, 4, COS_MERGE_LINEAR, NULL, 0,
                                NULL, 0, oracle_dist, &ctx,
                                scratch, out_best, &r_dest_sweep);

    printf("{\"kernel\":\"sigma_merge\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"methods\":[\"%s\",\"%s\",\"%s\",\"%s\"],"
             "\"constructive_single\":{"
                "\"method\":\"%s\",\"alpha\":%.4f,"
                "\"sigma_a\":%.4f,\"sigma_b\":%.4f,\"sigma_after\":%.4f,"
                "\"constructive\":%s},"
             "\"constructive_sweep\":{"
                "\"best_alpha\":%.4f,\"sigma_after\":%.4f,"
                "\"constructive\":%s},"
             "\"destructive_single\":{"
                "\"alpha\":0.5000,"
                "\"sigma_a\":%.4f,\"sigma_b\":%.4f,\"sigma_after\":%.4f,"
                "\"constructive\":%s},"
             "\"destructive_sweep\":{"
                "\"best_alpha\":%.4f,\"sigma_after\":%.4f,"
                "\"constructive\":%s}"
             "},"
           "\"pass\":%s}\n",
           rc,
           method_name(COS_MERGE_LINEAR),  method_name(COS_MERGE_SLERP),
           method_name(COS_MERGE_TIES),    method_name(COS_MERGE_TASK_ARITH),
           method_name(r_single.method), (double)r_single.best_alpha,
           (double)r_single.sigma_before_a,
           (double)r_single.sigma_before_b,
           (double)r_single.sigma_after,
           r_single.constructive ? "true" : "false",
           (double)r_sweep.best_alpha, (double)r_sweep.sigma_after,
           r_sweep.constructive ? "true" : "false",
           (double)r_dest_single.sigma_before_a,
           (double)r_dest_single.sigma_before_b,
           (double)r_dest_single.sigma_after,
           r_dest_single.constructive ? "true" : "false",
           (double)r_dest_sweep.best_alpha, (double)r_dest_sweep.sigma_after,
           r_dest_sweep.constructive ? "true" : "false",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Merge demo:\n");
        fprintf(stderr, "  constructive: α=%.2f σ_a=%.3f σ_b=%.3f σ_m=%.3f %s\n",
                (double)r_sweep.best_alpha,
                (double)r_sweep.sigma_before_a,
                (double)r_sweep.sigma_before_b,
                (double)r_sweep.sigma_after,
                r_sweep.constructive ? "CONSTRUCTIVE" : "DESTRUCTIVE");
        fprintf(stderr, "  destructive: α=%.2f σ_a=%.3f σ_b=%.3f σ_m=%.3f %s\n",
                (double)r_dest_sweep.best_alpha,
                (double)r_dest_sweep.sigma_before_a,
                (double)r_dest_sweep.sigma_before_b,
                (double)r_dest_sweep.sigma_after,
                r_dest_sweep.constructive ? "CONSTRUCTIVE" : "DESTRUCTIVE");
    }
    return rc;
}

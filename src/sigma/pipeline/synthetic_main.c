/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Synthetic self-test + canonical quality-gate + collapse demos. */

#include "synthetic.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *const *outputs;
    const float       *sigmas;
    const int         *is_real;
    int                n;
    int                cursor;
} script_t;

static int script_gen(const char *domain, int seed, void *ctx,
                      cos_synth_pair_t *out) {
    script_t *s = (script_t *)ctx;
    (void)domain; (void)seed;
    if (s->cursor >= s->n) return -1;
    out->input   = "q";
    out->output  = s->outputs[s->cursor];
    out->sigma   = s->sigmas [s->cursor];
    out->is_real = s->is_real[s->cursor];
    s->cursor++;
    return 0;
}

int main(int argc, char **argv) {
    int rc = cos_sigma_synth_self_test();

    /* --- Demo A: quality-gated acceptance, 80/20 anchor --- */
    const char *outs_a[] = {
        "alpha_fact_1", "beta_fact_2", "gamma_fact_3", "delta_fact_4",
        "epsilon_fact_5", "zeta_fact_6", "eta_fact_7", "theta_fact_8",
        "iota_fact_9",  "REAL_anchor_1"
    };
    float sigmas_a[]  = {0.10f, 0.15f, 0.80f, 0.20f, 0.90f,
                         0.25f, 0.12f, 0.18f, 0.95f, 0.05f};
    int   real_a[]    = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    script_t sa = {outs_a, sigmas_a, real_a, 10, 0};

    cos_synth_config_t cfg_a;
    cos_sigma_synth_init(&cfg_a, 0.30f, 0.10f, 0.20f, 30);
    cos_synth_pair_t pairs_a[5];
    cos_synth_report_t rep_a;
    cos_sigma_synth_generate(&cfg_a, "factual", 5, script_gen, &sa,
                             pairs_a, 5, &rep_a);

    /* --- Demo B: collapse detector, all outputs share 8-byte prefix --- */
    const char *outs_b[] = {
        "collapse_01", "collapse_02", "collapse_03",
        "collapse_04", "collapse_05"
    };
    float sigmas_b[] = {0.10f, 0.10f, 0.10f, 0.10f, 0.10f};
    int   real_b[]   = {0, 0, 0, 0, 0};
    script_t sb = {outs_b, sigmas_b, real_b, 5, 0};
    cos_synth_config_t cfg_b;
    cos_sigma_synth_init(&cfg_b, 0.30f, 0.50f, 0.0f, 20);
    cos_synth_pair_t pairs_b[5];
    cos_synth_report_t rep_b;
    cos_sigma_synth_generate(&cfg_b, "collapse", 5, script_gen, &sb,
                             pairs_b, 5, &rep_b);

    printf("{\"kernel\":\"sigma_synthetic\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"quality_gate\":{"
               "\"n_target\":%d,\"attempts\":%d,\"accepted\":%d,"
               "\"n_real\":%d,\"n_synthetic\":%d,"
               "\"rejected_quality\":%d,\"rejected_anchor\":%d,"
               "\"acceptance_rate\":%.4f,\"diversity\":%.4f,"
               "\"collapsed\":%s},"
             "\"collapse_guard\":{"
               "\"accepted\":%d,\"attempts\":%d,"
               "\"diversity\":%.4f,\"collapsed\":%s}"
             "},"
           "\"pass\":%s}\n",
           rc,
           rep_a.n_target, rep_a.attempts, rep_a.accepted,
           rep_a.n_real, rep_a.n_synthetic,
           rep_a.rejected_quality, rep_a.rejected_anchor,
           (double)rep_a.acceptance_rate, (double)rep_a.diversity,
           rep_a.collapsed ? "true" : "false",
           rep_b.accepted, rep_b.attempts,
           (double)rep_b.diversity, rep_b.collapsed ? "true" : "false",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Synthetic demo:\n");
        fprintf(stderr, "  qA: accepted=%d/%d rate=%.2f diversity=%.2f\n",
                rep_a.accepted, rep_a.attempts,
                (double)rep_a.acceptance_rate, (double)rep_a.diversity);
        fprintf(stderr, "  qB: collapsed=%s accepted=%d diversity=%.2f\n",
                rep_b.collapsed ? "yes" : "no",
                rep_b.accepted, (double)rep_b.diversity);
    }
    return rc;
}

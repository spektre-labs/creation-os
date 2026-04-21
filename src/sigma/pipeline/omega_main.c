/* σ-Omega self-test + canonical Ω-loop demo.
 *
 * The demo runs 8 iterations of a deterministic stub where
 * σ_integral falls by 0.05 each step.  A forced K_eff drop on
 * iteration 5 triggers a single rollback; the loop recovers and
 * continues monotonically afterward. */

#include "omega.h"
#include "omega_agi_live.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int   step;
    float penalty;
    int   force_k_drop_at;
    int   n_save;
    int   n_restore;
} demo_t;

static float d_bench(cos_omega_config_t *cfg, void *ctx) {
    (void)cfg;
    demo_t *d = (demo_t *)ctx;
    float v = 0.80f - 0.05f * (float)d->step + d->penalty;
    if (v < 0.01f) v = 0.01f;
    d->penalty *= 0.5f;
    d->step++;
    return v;
}

static float d_k(cos_omega_config_t *cfg, void *ctx) {
    (void)cfg;
    demo_t *d = (demo_t *)ctx;
    if (d->step - 1 == d->force_k_drop_at) return 0.01f;
    return 0.20f;
}

static int d_save(const cos_omega_config_t *cfg, void *ctx,
                  void *out, size_t cap) {
    (void)cfg;
    demo_t *d = (demo_t *)ctx;
    if (cap < sizeof *d) return -1;
    memcpy(out, d, sizeof *d);
    d->n_save++;
    return (int)sizeof *d;
}

static int d_restore(cos_omega_config_t *cfg, void *ctx,
                     const void *buf, size_t n) {
    (void)cfg; (void)buf; (void)n;
    demo_t *d = (demo_t *)ctx;
    d->penalty  = 0.04f;
    d->n_restore++;
    return 0;
}

static int d_meta(cos_omega_config_t *cfg, void *ctx,
                  char *out_focus, size_t cap) {
    (void)cfg; (void)ctx;
    snprintf(out_focus, cap, "%s", "math");
    return 0;
}

int main(int argc, char **argv) {
    int agi = cos_agi_omega_live_dispatch(argc, argv);
    if (agi != -1)
        return agi;

    int rc = cos_sigma_omega_self_test();

    cos_omega_hooks_t h = {0};
    h.benchmark_sigma_integral = d_bench;
    h.compute_k_eff            = d_k;
    h.save_snapshot            = d_save;
    h.restore_snapshot         = d_restore;
    h.meta_assess              = d_meta;

    cos_omega_policy_t p;
    cos_sigma_omega_policy_default(&p);
    p.n_iterations    = 8;
    p.selfplay_rounds = 0;
    p.k_crit          = 0.05f;

    demo_t d = { .step = 0, .penalty = 0.0f, .force_k_drop_at = 5 };
    unsigned char buf[sizeof(demo_t)];
    cos_omega_iter_log_t trace[8];
    cos_omega_report_t   report;
    int dummy = 0;
    cos_sigma_omega_run((cos_omega_config_t *)&dummy, &h, &d, &p,
                        buf, sizeof buf, trace, 8, &report);

    int rollback_iter = -1;
    for (int i = 0; i < report.iterations; ++i) {
        if (trace[i].rolled_back) { rollback_iter = i; break; }
    }

    printf("{\"kernel\":\"sigma_omega\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"iterations\":%d,\"best_iter\":%d,"
             "\"best_sigma\":%.4f,\"last_sigma\":%.4f,"
             "\"mean_sigma\":%.4f,\"k_eff_min\":%.4f,"
             "\"n_improvements\":%d,\"n_rollbacks\":%d,"
             "\"n_save\":%d,\"n_restore\":%d,"
             "\"focus\":\"%s\","
             "\"trace_rollback_iter\":%d"
             "},"
           "\"pass\":%s}\n",
           rc,
           report.iterations, report.best_iter,
           (double)report.best_sigma, (double)report.last_sigma,
           (double)report.mean_sigma, (double)report.k_eff_min,
           report.n_improvements, report.n_rollbacks,
           d.n_save, d.n_restore,
           trace[report.iterations - 1].focus,
           rollback_iter,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Omega demo: iters=%d best=%.3f rollbacks=%d\n",
                report.iterations, (double)report.best_sigma,
                report.n_rollbacks);
    }
    return rc;
}

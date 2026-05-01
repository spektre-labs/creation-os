/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Omega primitive — recursive Ω = argmin ∫σ dt, K ≥ K_crit. */

#include "omega.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void cos_sigma_omega_policy_default(cos_omega_policy_t *p) {
    if (!p) return;
    p->n_iterations    = 10;
    p->selfplay_rounds = 100;
    p->k_crit          = 0.05f;
    p->improvement_eps = 1e-5f;
}

int cos_sigma_omega_run(cos_omega_config_t *cfg,
                        const cos_omega_hooks_t *hooks,
                        void *ctx,
                        const cos_omega_policy_t *policy,
                        void *snap, size_t snap_cap,
                        cos_omega_iter_log_t *trace, int trace_cap,
                        cos_omega_report_t *report)
{
    if (!cfg || !hooks || !policy || !report) return -1;
    if (!hooks->benchmark_sigma_integral)      return -2;
    if (policy->n_iterations < 1)              return -3;

    memset(report, 0, sizeof *report);
    report->best_sigma = INFINITY;
    report->k_eff_min  = INFINITY;
    report->trace      = trace;
    report->trace_cap  = trace_cap;

    double sigma_sum = 0.0;
    int    snap_ok   = 0;
    size_t snap_n    = 0;

    for (int i = 0; i < policy->n_iterations; ++i) {
        /* [1] Optional sub-loops BEFORE measurement so the
         * benchmark sees the new state. */
        if (hooks->selfplay_batch)
            hooks->selfplay_batch(cfg, ctx, policy->selfplay_rounds);
        if (hooks->ttt_batch)
            hooks->ttt_batch(cfg, ctx);
        if (hooks->evolve_step)
            hooks->evolve_step(cfg, ctx);

        char focus[32] = {0};
        if (hooks->meta_assess)
            hooks->meta_assess(cfg, ctx, focus, sizeof focus);

        /* [2] Measure. */
        float sigma = hooks->benchmark_sigma_integral(cfg, ctx);
        if (sigma != sigma)     sigma = 1.0f;  /* NaN guard         */
        if (sigma < 0.0f)       sigma = 0.0f;

        /* [3] Update best snapshot before K_eff check — if K_eff
         * is below floor later we want to fall back to this one. */
        int improved = 0;
        if (sigma + policy->improvement_eps < report->best_sigma) {
            report->best_sigma = sigma;
            report->best_iter  = i;
            report->n_improvements++;
            improved = 1;
            if (hooks->save_snapshot && snap && snap_cap > 0) {
                int n = hooks->save_snapshot(cfg, ctx, snap, snap_cap);
                if (n > 0) { snap_ok = 1; snap_n = (size_t)n; }
            }
        }

        /* [4] K_eff safety: if below floor, restore best snapshot
         * and flag this iteration as a rollback. */
        int rolled = 0;
        float k = INFINITY;
        if (hooks->compute_k_eff) {
            k = hooks->compute_k_eff(cfg, ctx);
            if (k != k) k = 0.0f;   /* NaN → worst case */
            if (k < report->k_eff_min) report->k_eff_min = k;
            if (k < policy->k_crit && snap_ok && hooks->restore_snapshot) {
                hooks->restore_snapshot(cfg, ctx, snap, snap_n);
                report->n_rollbacks++;
                rolled = 1;
            }
        }

        sigma_sum += (double)sigma;
        report->last_sigma = sigma;
        report->iterations = i + 1;

        if (trace && i < trace_cap) {
            trace[i].iteration       = i;
            trace[i].sigma_integral  = sigma;
            trace[i].k_eff           = k;
            trace[i].improved        = improved;
            trace[i].rolled_back     = rolled;
            strncpy(trace[i].focus, focus, sizeof trace[i].focus - 1);
            trace[i].focus[sizeof trace[i].focus - 1] = '\0';
        }
    }

    report->mean_sigma = report->iterations > 0
        ? (float)(sigma_sum / (double)report->iterations) : 0.0f;
    return 0;
}

/* ---------- self-test ---------- */

typedef struct {
    int    step;
    float  sigma_base;    /* monotonically decreases                */
    float  sigma_penalty; /* set by rollback; decays                */
    int    force_k_drop_at;
    float  k_crit_value;
    int    saved_step;
    float  saved_sigma;
    int    restored;
} stub_t;

static float stub_bench(cos_omega_config_t *cfg, void *ctx) {
    (void)cfg;
    stub_t *s = (stub_t *)ctx;
    /* σ_integral falls by 0.05 per step, plus any active penalty. */
    float val = 0.80f - 0.05f * (float)s->step + s->sigma_penalty;
    if (val < 0.01f) val = 0.01f;
    s->sigma_penalty *= 0.5f;
    s->step++;
    return val;
}

static float stub_k(cos_omega_config_t *cfg, void *ctx) {
    (void)cfg;
    stub_t *s = (stub_t *)ctx;
    /* K_eff drops below floor on the forced step, then recovers. */
    if (s->step - 1 == s->force_k_drop_at) return 0.01f;
    return 0.20f;
}

static int stub_save(const cos_omega_config_t *cfg, void *ctx,
                     void *out, size_t cap) {
    (void)cfg;
    stub_t *s = (stub_t *)ctx;
    if (cap < sizeof *s) return -1;
    memcpy(out, s, sizeof *s);
    s->saved_step  = s->step;
    s->saved_sigma = 0.80f - 0.05f * (float)s->step;
    return (int)sizeof *s;
}

static int stub_restore(cos_omega_config_t *cfg, void *ctx,
                        const void *buf, size_t n) {
    (void)cfg; (void)n;
    stub_t *s = (stub_t *)ctx;
    /* Simulate reverting by re-applying a penalty so the next
     * benchmark does NOT trivially beat best. */
    (void)buf;
    s->sigma_penalty = 0.04f;
    s->restored     += 1;
    return 0;
}

static int check_policy_defaults(void) {
    cos_omega_policy_t p;
    cos_sigma_omega_policy_default(&p);
    if (p.n_iterations    <= 0)    return 10;
    if (p.selfplay_rounds <= 0)    return 11;
    if (!(p.k_crit > 0.0f))        return 12;
    if (!(p.improvement_eps > 0))  return 13;
    return 0;
}

static int check_monotone_best(void) {
    /* 6 iterations, K never drops → no rollback, best_sigma
     * monotonically non-increasing. */
    cos_omega_hooks_t h = {0};
    h.benchmark_sigma_integral = stub_bench;
    h.compute_k_eff            = stub_k;
    h.save_snapshot            = stub_save;
    h.restore_snapshot         = stub_restore;

    cos_omega_policy_t p = { .n_iterations = 6, .selfplay_rounds = 0,
                             .k_crit = 0.05f, .improvement_eps = 1e-5f };
    stub_t s = { .step = 0, .force_k_drop_at = -1, .k_crit_value = 0.05f };
    char buf[128];
    cos_omega_iter_log_t trace[6];
    cos_omega_report_t  rep;

    int dummy = 0;
    if (cos_sigma_omega_run((cos_omega_config_t *)&dummy, &h, &s, &p,
                             buf, sizeof buf, trace, 6, &rep) != 0) return 20;
    if (rep.iterations != 6)                    return 21;
    if (rep.n_rollbacks != 0)                   return 22;
    if (rep.n_improvements != 6)                return 23;
    /* best_sigma ≤ every trace σ_integral. */
    for (int i = 0; i < 6; ++i) {
        if (trace[i].sigma_integral + 1e-5f < rep.best_sigma) return 24;
    }
    /* Monotone non-increasing best — each step either holds or drops. */
    float last = 1e9f;
    for (int i = 0; i < 6; ++i) {
        float v = trace[i].sigma_integral;
        if (v > last + 1e-5f) return 25;
        last = v;
    }
    return 0;
}

static int check_rollback(void) {
    /* Force K_eff to drop at iteration 3 → expect a rollback
     * stamped exactly there. */
    cos_omega_hooks_t h = {0};
    h.benchmark_sigma_integral = stub_bench;
    h.compute_k_eff            = stub_k;
    h.save_snapshot            = stub_save;
    h.restore_snapshot         = stub_restore;

    cos_omega_policy_t p = { .n_iterations = 6, .selfplay_rounds = 0,
                             .k_crit = 0.05f, .improvement_eps = 1e-5f };
    stub_t s = { .step = 0, .force_k_drop_at = 3, .k_crit_value = 0.05f };
    char buf[128];
    cos_omega_iter_log_t trace[6];
    cos_omega_report_t  rep;
    int dummy = 0;
    cos_sigma_omega_run((cos_omega_config_t *)&dummy, &h, &s, &p,
                        buf, sizeof buf, trace, 6, &rep);
    if (rep.n_rollbacks != 1)           return 30;
    if (!trace[3].rolled_back)          return 31;
    if (rep.k_eff_min > 0.02f)          return 32;
    if (s.restored != 1)                return 33;
    return 0;
}

int cos_sigma_omega_self_test(void) {
    int rc;
    if ((rc = check_policy_defaults())) return rc;
    if ((rc = check_monotone_best()))   return rc;
    if ((rc = check_rollback()))        return rc;
    return 0;
}

/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "evolver.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_evolver_stderr_verbose;

void cos_evolver_set_stderr_verbose(int on)
{
    g_evolver_stderr_verbose = on ? 1 : 0;
}

void cos_evolver_config_default(cos_evolver_config_t *c)
{
    if (c == NULL)
        return;
    memset(c, 0, sizeof(*c));
    c->tau_accept          = 0.40f;
    c->tau_rethink         = 0.60f;
    c->semantic_temp_low   = 0.10f;
    c->semantic_temp_high  = 1.50f;
    c->semantic_max_tokens = 60;
    c->preferred_tier      = 0;
}

cos_evolver_config_t cos_evolver_step(cos_evolver_config_t current,
                                       const float *sigma_history, int n_history,
                                       const int *correct_history, int n_correct)
{
    cos_evolver_config_t next = current;
    if (sigma_history == NULL || correct_history == NULL || n_history <= 0
        || n_correct != n_history) {
        return next;
    }

    float sigma_mean       = 0.f;
    int   abstain_count    = 0;
    int   wrong_confident  = 0;
    int   i;

    for (i = 0; i < n_history; i++) {
        sigma_mean += sigma_history[i];
        if (sigma_history[i] > current.tau_rethink)
            abstain_count++;
        /* "Confident" side of the gate: σ below τ_accept; disagree with ACCEPT */
        if (sigma_history[i] < current.tau_accept && !correct_history[i])
            wrong_confident++;
    }
    sigma_mean /= (float)n_history;

    if (wrong_confident > 0) {
        next.tau_accept *= 0.9f;
        if (next.tau_accept < 0.05f)
            next.tau_accept = 0.05f;
        if (g_evolver_stderr_verbose)
            fprintf(stderr,
                    "[evolver] wrong+confident=%d → τ_accept %.3f → %.3f\n",
                    wrong_confident, (double)current.tau_accept,
                    (double)next.tau_accept);
    }

    {
        float abstain_rate = (float)abstain_count / (float)n_history;
        if (abstain_rate > 0.30f) {
            next.tau_rethink *= 1.1f;
            if (next.tau_rethink > 0.95f)
                next.tau_rethink = 0.95f;
            if (g_evolver_stderr_verbose)
                fprintf(stderr,
                        "[evolver] abstain=%.0f%% → τ_rethink %.3f → %.3f\n",
                        (double)(abstain_rate * 100.0f),
                        (double)current.tau_rethink, (double)next.tau_rethink);
        }
    }

    /* Rule 4: rising batch mean vs first half — widen semantic temperature span */
    if (n_history >= 6) {
        float m0 = 0.f, m1 = 0.f;
        int   h  = n_history / 2;
        for (i = 0; i < h; i++)
            m0 += sigma_history[i];
        for (; i < n_history; i++)
            m1 += sigma_history[i];
        m0 /= (float)h;
        m1 /= (float)(n_history - h);
        if (m1 > m0 + 0.05f) {
            next.semantic_temp_low *= 0.95f;
            next.semantic_temp_high *= 1.05f;
            if (next.semantic_temp_low < 0.01f)
                next.semantic_temp_low = 0.01f;
            if (next.semantic_temp_high > 2.0f)
                next.semantic_temp_high = 2.0f;
            if (g_evolver_stderr_verbose)
                fprintf(stderr,
                        "[evolver] σ_mean rising (%.3f→%.3f) → semantic temps "
                        "%.3f/%.3f\n",
                        (double)m0, (double)m1, (double)next.semantic_temp_low,
                        (double)next.semantic_temp_high);
        }
    }

    (void)sigma_mean;
    return next;
}

void cos_evolver_emit_jsonl(FILE *fp, int64_t t_ms, int turn,
                            const cos_evolver_config_t *prev,
                            const cos_evolver_config_t *next)
{
    if (fp == NULL || prev == NULL || next == NULL)
        return;
    fprintf(fp,
            "{\"t\":%lld,\"evolver\":1,\"turn\":%d,"
            "\"tau_accept\":%.4f,\"tau_rethink\":%.4f,"
            "\"semantic_temp_low\":%.4f,\"semantic_temp_high\":%.4f,"
            "\"semantic_max_tokens\":%d,\"preferred_tier\":%d,"
            "\"prev_tau_accept\":%.4f,\"prev_tau_rethink\":%.4f}\n",
            (long long)t_ms, turn, (double)next->tau_accept,
            (double)next->tau_rethink, (double)next->semantic_temp_low,
            (double)next->semantic_temp_high, next->semantic_max_tokens,
            next->preferred_tier, (double)prev->tau_accept,
            (double)prev->tau_rethink);
    fflush(fp);
}

int cos_evolver_self_test(void)
{
    cos_evolver_config_t c, n;
    float                sig[4] = { 0.2f, 0.85f, 0.9f, 0.88f };
    int                  ok[4]  = { 1, 0, 0, 0 };

    cos_evolver_config_default(&c);
    n = cos_evolver_step(c, sig, 4, ok, 4);
    if (n.tau_rethink <= c.tau_rethink + 1e-6f)
        return 1;
    cos_evolver_config_default(&c);
    {
        float s2[3] = { 0.15f, 0.12f, 0.18f };
        int   k2[3] = { 0, 0, 0 };
        n           = cos_evolver_step(c, s2, 3, k2, 3);
        if (n.tau_accept >= c.tau_accept - 1e-6f)
            return 2;
    }
    return 0;
}

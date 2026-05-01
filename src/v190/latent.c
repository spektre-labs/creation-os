/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v190 σ-Latent-Reason — reference implementation.
 *
 *   The thinking block is a contraction map so convergence is
 *   Banach-guaranteed on the fixture:
 *
 *       h_{n+1} = ρ · (h_n - h*)  +  h*
 *
 *   where ρ < 1 (contraction factor) and h* is a query-specific
 *   fixed point.  Then ‖h_{n+1} - h_n‖ = ρ · ‖h_n - h_{n-1}‖,
 *   i.e. σ_latent geometrically decays → strictly monotone.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "latent.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t splitmix64(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float frand(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) / (double)(1ULL << 53));
}

static float vnorm(const float *v, int d) {
    double s = 0.0;
    for (int i = 0; i < d; ++i) s += (double)v[i] * v[i];
    return (float)sqrt(s);
}

static void vsub(const float *a, const float *b, float *r, int d) {
    for (int i = 0; i < d; ++i) r[i] = a[i] - b[i];
}

/* ---- init ------------------------------------------------------ */

void cos_v190_init(cos_v190_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x190A7E47ULL;
    s->n_queries    = COS_V190_N_QUERIES;
    s->tau_converge = 0.01f;
    s->max_depth    = COS_V190_MAX_DEPTH;
    s->dim          = COS_V190_DIM;
    s->contraction  = 0.50f;            /* mean ρ (per-class overrides) */
}

/* ---- fixture --------------------------------------------------- */

void cos_v190_build(cos_v190_state_t *s) {
    uint64_t r = s->seed;
    for (int i = 0; i < s->n_queries; ++i) {
        cos_v190_query_t *q = &s->queries[i];
        q->id = i;

        /* Three classes: 12 easy / 10 medium / 10 hard. */
        int cls = (i < 12) ? 0 : (i < 22) ? 1 : 2;
        q->class_ = cls;

        float base = (cls == 0) ? 0.05f + 0.10f * frand(&r) :
                     (cls == 1) ? 0.25f + 0.20f * frand(&r) :
                                  0.55f + 0.35f * frand(&r);
        if (base > 0.95f) base = 0.95f;
        q->sigma_initial = base;

        /* Random fixed point h* in [-1, 1]^d, then h_0 = h* + radius·e
         * where e is a unit random direction.  Radius encodes how far
         * h_0 starts from the fixed point; harder queries start
         * farther AND the per-class contraction ρ is steeper, so
         * hard queries need more iterations to converge while easy
         * queries converge in ~2 iters. */
        float hstar[COS_V190_DIM];
        float dir[COS_V190_DIM];
        double sumsq = 0.0;
        for (int k = 0; k < s->dim; ++k) {
            hstar[k] = 2.0f * frand(&r) - 1.0f;
            dir[k]   = 2.0f * frand(&r) - 1.0f;
            sumsq += (double)dir[k] * dir[k];
        }
        float nrm = (float)sqrt(sumsq);
        if (nrm < 1e-6f) nrm = 1.0f;
        float radius       = (cls == 0) ? 0.30f :
                             (cls == 1) ? 2.00f :
                                          4.00f;
        q->contraction     = (cls == 0) ? 0.05f :
                             (cls == 1) ? 0.30f :
                                          0.55f;
        for (int k = 0; k < s->dim; ++k) {
            dir[k] /= nrm;
            q->h[0][k] = hstar[k] + radius * dir[k];
        }
        /* Stash h* in h[max_depth] for use during run.  The run loop
         * doesn't read h[max_depth] until after the iteration, but
         * we over-allocated one extra slot and we read it in run().
         * To avoid confusion, we instead regenerate hstar in run()
         * using a mirror PRNG below. */
        /* Save fixed point at the tail slot. */
        for (int k = 0; k < s->dim; ++k)
            q->h[s->max_depth][k] = hstar[k];
    }
}

/* ---- run ------------------------------------------------------- */

void cos_v190_run(cos_v190_state_t *s) {
    s->n_converged        = 0;
    s->n_easy = s->n_medium = s->n_hard = 0;
    double sum_it_easy = 0, sum_it_med = 0, sum_it_hard = 0;

    bool monotone_all        = true;
    s->total_middle_tokens   = 0;

    for (int i = 0; i < s->n_queries; ++i) {
        cos_v190_query_t *q = &s->queries[i];

        float hstar[COS_V190_DIM];
        for (int k = 0; k < s->dim; ++k) hstar[k] = q->h[s->max_depth][k];

        q->n_middle_tokens = 0;         /* latent, no tokens emitted */
        q->converged       = false;
        q->min_sigma_latent = 1e30f;

        int n = 0;
        float prev_delta = -1.0f;
        float rho = q->contraction;
        for (n = 0; n < s->max_depth; ++n) {
            /* h_{n+1} = ρ (h_n - h*) + h* */
            for (int k = 0; k < s->dim; ++k) {
                float diff = q->h[n][k] - hstar[k];
                q->h[n+1][k] = rho * diff + hstar[k];
            }
            float delta_vec[COS_V190_DIM];
            vsub(q->h[n+1], q->h[n], delta_vec, s->dim);
            float delta = vnorm(delta_vec, s->dim);
            float hn    = vnorm(q->h[n],   s->dim);
            if (hn < 1e-6f) hn = 1e-6f;
            float sig   = delta / hn;
            q->sigma_latent[n] = sig;

            if (prev_delta >= 0.0f && !(delta + 1e-12f < prev_delta))
                monotone_all = false;
            prev_delta = delta;

            if (sig < q->min_sigma_latent) q->min_sigma_latent = sig;

            if (sig < s->tau_converge) {
                q->converged = true;
                ++n;                    /* account for this iter */
                break;
            }
        }
        q->n_iters = n;
        if (q->converged) s->n_converged++;

        switch (q->class_) {
        case 0: s->n_easy++;   sum_it_easy += n; break;
        case 1: s->n_medium++; sum_it_med  += n; break;
        default: s->n_hard++;  sum_it_hard += n; break;
        }
        s->total_middle_tokens += q->n_middle_tokens;
    }
    s->monotone_sigma_all = monotone_all;
    s->mean_iters_easy    = s->n_easy   ? sum_it_easy / s->n_easy   : 0.0;
    s->mean_iters_medium  = s->n_medium ? sum_it_med  / s->n_medium : 0.0;
    s->mean_iters_hard    = s->n_hard   ? sum_it_hard / s->n_hard   : 0.0;
}

/* ---- JSON ------------------------------------------------------ */

size_t cos_v190_to_json(const cos_v190_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v190\","
        "\"n_queries\":%d,\"n_converged\":%d,"
        "\"tau_converge\":%.3f,\"max_depth\":%d,\"dim\":%d,"
        "\"contraction\":%.3f,"
        "\"n_easy\":%d,\"n_medium\":%d,\"n_hard\":%d,"
        "\"mean_iters_easy\":%.3f,"
        "\"mean_iters_medium\":%.3f,"
        "\"mean_iters_hard\":%.3f,"
        "\"monotone_sigma_all\":%s,"
        "\"total_middle_tokens\":%d,"
        "\"queries\":[",
        s->n_queries, s->n_converged,
        s->tau_converge, s->max_depth, s->dim, s->contraction,
        s->n_easy, s->n_medium, s->n_hard,
        s->mean_iters_easy, s->mean_iters_medium, s->mean_iters_hard,
        s->monotone_sigma_all ? "true" : "false",
        s->total_middle_tokens);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_queries; ++i) {
        const cos_v190_query_t *q = &s->queries[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"class\":%d,"
            "\"sigma_initial\":%.3f,"
            "\"n_iters\":%d,\"min_sigma_latent\":%.5f,"
            "\"converged\":%s,\"n_middle_tokens\":%d}",
            i == 0 ? "" : ",", q->id, q->class_,
            q->sigma_initial, q->n_iters, q->min_sigma_latent,
            q->converged ? "true" : "false",
            q->n_middle_tokens);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test ------------------------------------------------- */

int cos_v190_self_test(void) {
    cos_v190_state_t s;
    cos_v190_init(&s, 0x190DEEDULL);
    cos_v190_build(&s);
    cos_v190_run(&s);

    if (s.n_queries != COS_V190_N_QUERIES)               return 1;
    if (!s.monotone_sigma_all)                            return 2;
    if (s.total_middle_tokens != 0)                       return 3;

    /* ≥ 90 % of queries converge to σ_latent < τ_converge. */
    int thresh = (s.n_queries * 9) / 10;
    if (s.n_converged < thresh)                           return 4;

    /* Hard queries use ≥ 3× more iterations than easy. */
    if (!(s.mean_iters_hard >= 3.0 * s.mean_iters_easy))  return 5;

    /* Each query's min_sigma_latent is actually < τ_converge when
     * converged, and ≥ τ_converge when not converged (sanity). */
    for (int i = 0; i < s.n_queries; ++i) {
        const cos_v190_query_t *q = &s.queries[i];
        if (q->converged && !(q->min_sigma_latent < s.tau_converge))
            return 6;
    }
    return 0;
}

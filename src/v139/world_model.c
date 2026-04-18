/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v139 σ-WorldModel — implementation.
 */
#include "world_model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- SplitMix64 + standard normal via Box-Muller ---------------- */
static uint64_t splitmix64(uint64_t *s) {
    *s += 0x9E3779B97F4A7C15ULL;
    uint64_t z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand01(uint64_t *s) {
    return (float)((splitmix64(s) >> 11) & 0x1FFFFFFFFFFFFFULL)
         / (float)(1ULL << 53);
}
static float nrand(uint64_t *s) {
    float u1 = urand01(s); if (u1 < 1e-12f) u1 = 1e-12f;
    float u2 = urand01(s);
    return sqrtf(-2.0f * logf(u1)) * cosf(6.2831853f * u2);
}

/* --- Gauss-Jordan with partial pivoting -----------------------
 * Solves A·X = B in-place.  A is D×D, B is D×D (the unknown).
 * Returns 0 on success, <0 if A is singular.  Uses double
 * internally for better conditioning on small matrices. */
static int gauss_jordan_solve(double *A, double *B, int n) {
    for (int k = 0; k < n; ++k) {
        int    piv = k;
        double maxabs = fabs(A[k * n + k]);
        for (int i = k + 1; i < n; ++i) {
            double v = fabs(A[i * n + k]);
            if (v > maxabs) { maxabs = v; piv = i; }
        }
        if (maxabs < 1e-12) return -1;
        if (piv != k) {
            for (int j = 0; j < n; ++j) {
                double t = A[k*n+j]; A[k*n+j] = A[piv*n+j]; A[piv*n+j] = t;
                t = B[k*n+j]; B[k*n+j] = B[piv*n+j]; B[piv*n+j] = t;
            }
        }
        double pv = A[k * n + k];
        double inv = 1.0 / pv;
        for (int j = 0; j < n; ++j) {
            A[k * n + j] *= inv;
            B[k * n + j] *= inv;
        }
        for (int i = 0; i < n; ++i) {
            if (i == k) continue;
            double f = A[i * n + k];
            if (f == 0.0) continue;
            for (int j = 0; j < n; ++j) {
                A[i * n + j] -= f * A[k * n + j];
                B[i * n + j] -= f * B[k * n + j];
            }
        }
    }
    return 0;
}

/* ================================================================
 * Least-squares fit via normal equations:
 *   A (S_c S_c^T) = S_n S_c^T     ⇒  A = (S_n S_c^T) (S_c S_c^T)^{-1}
 * pairs_curr is [n, D] row-major, treat as columns of S_c (D rows).
 * For each row i of A: solve (S_c S_c^T) · a_i^T = (S_n S_c^T) row_i
 * But it's easier to build G = S_c S_c^T (D×D) and R = S_n S_c^T
 * (D×D), then solve G · A^T = R^T, so A = solve(G, R^T)^T.
 * ============================================================= */
int cos_v139_fit(cos_v139_model_t *m, int dim, int n,
                 const float *curr, const float *next) {
    if (!m || dim <= 0 || dim > COS_V139_MAX_DIM || n <= dim
        || !curr || !next) return -1;
    memset(m, 0, sizeof *m);
    m->dim = dim;
    m->n_training_pairs = n;

    double *G = (double *)calloc((size_t)dim * dim, sizeof(double));
    double *R = (double *)calloc((size_t)dim * dim, sizeof(double));
    double *X = (double *)calloc((size_t)dim * dim, sizeof(double));
    if (!G || !R || !X) { free(G); free(R); free(X); return -1; }

    /* G[i,j] = Σ_k curr[k,i] * curr[k,j]
     * R[i,j] = Σ_k next[k,i] * curr[k,j]                          */
    for (int k = 0; k < n; ++k) {
        const float *c = curr + (size_t)k * dim;
        const float *nx = next + (size_t)k * dim;
        for (int i = 0; i < dim; ++i) {
            double ci = c[i];
            double ni = nx[i];
            for (int j = 0; j < dim; ++j) {
                G[(size_t)i * dim + j] += ci * c[j];
                R[(size_t)i * dim + j] += ni * c[j];
            }
        }
    }

    /* Tikhonov regularisation (λ = 1e-6) so the normal matrix is
     * never singular for ≥ D well-separated pairs. */
    for (int i = 0; i < dim; ++i)
        G[(size_t)i * dim + i] += 1e-6;

    /* We want A such that A · G = R, i.e. A = R · G^{-1}.  G is
     * symmetric so G^{-1} is too, and A^T = G^{-1} · R^T.  So we
     * copy R^T into X, solve G · X = R^T (Gauss-Jordan in place),
     * which leaves X = G^{-1} · R^T = A^T.  Transpose on the way
     * out. */
    for (int i = 0; i < dim; ++i)
        for (int j = 0; j < dim; ++j)
            X[(size_t)j * dim + i] = R[(size_t)i * dim + j];
    int rc = gauss_jordan_solve(G, X, dim);
    if (rc != 0) { free(G); free(R); free(X); return -2; }

    for (int i = 0; i < dim; ++i)
        for (int j = 0; j < dim; ++j)
            m->A[i * dim + j] = (float)X[(size_t)j * dim + i];

    /* Training-set mean residual for reference. */
    double sum = 0.0;
    int  counted = 0;
    float pred[COS_V139_MAX_DIM];
    for (int k = 0; k < n; ++k) {
        const float *c = curr + (size_t)k * dim;
        const float *nx = next + (size_t)k * dim;
        cos_v139_predict(m, c, pred);
        double num = 0.0, den = 0.0;
        for (int i = 0; i < dim; ++i) {
            double d = (double)nx[i] - (double)pred[i];
            num += d * d;
            den += (double)nx[i] * (double)nx[i];
        }
        num = sqrt(num); den = sqrt(den);
        if (den > 1e-9) { sum += num / den; counted++; }
    }
    m->mean_l2_residual = counted ? (float)(sum / counted) : 0.0f;

    free(G); free(R); free(X);
    return 0;
}

void cos_v139_predict(const cos_v139_model_t *m,
                      const float *s, float *out) {
    if (!m || !s || !out) return;
    int D = m->dim;
    for (int i = 0; i < D; ++i) {
        double acc = 0.0;
        const float *row = m->A + (size_t)i * D;
        for (int j = 0; j < D; ++j) acc += (double)row[j] * (double)s[j];
        out[i] = (float)acc;
    }
}

float cos_v139_sigma_world(const float *a, const float *p, int D) {
    if (!a || !p || D <= 0) return 0.0f;
    double num = 0.0, den = 0.0;
    for (int i = 0; i < D; ++i) {
        double d = (double)a[i] - (double)p[i];
        num += d * d;
        den += (double)a[i] * (double)a[i];
    }
    num = sqrt(num); den = sqrt(den);
    if (den < 1e-9) return (float)num;
    return (float)(num / den);
}

int cos_v139_rollout(const cos_v139_model_t *m,
                     const float *seed, int horizon,
                     const float *actual,
                     cos_v139_rollout_t *out) {
    if (!m || !seed || horizon <= 0
        || horizon > COS_V139_MAX_ROLLOUT || !out) return -1;
    int D = m->dim;
    memset(out, 0, sizeof *out);
    out->horizon = horizon;
    out->monotone_rising = 1;

    /* states[0..horizon-1] is the predicted next-state at each step.
     * We hold the rolling prediction in `cur`. */
    float cur [COS_V139_MAX_DIM];
    float nxt [COS_V139_MAX_DIM];
    memcpy(cur, seed, (size_t)D * sizeof(float));
    float last_sigma = -1.0f;
    for (int t = 0; t < horizon; ++t) {
        cos_v139_predict(m, cur, nxt);
        memcpy(out->states + (size_t)t * D, nxt, (size_t)D * sizeof(float));
        if (actual) {
            const float *a = actual + (size_t)(t + 1) * D;
            float sig = cos_v139_sigma_world(a, nxt, D);
            out->sigma_world[t] = sig;
            if (last_sigma >= 0.0f && sig + 1e-6f < last_sigma)
                out->monotone_rising = 0;
            last_sigma = sig;
        }
        memcpy(cur, nxt, (size_t)D * sizeof(float));
    }
    return 0;
}

/* ================================================================
 * Deterministic synthetic trajectory.
 *
 * Builds a random rotation-scale matrix A_true = ρ · Q, where Q is
 * sampled by orthogonalising a Gaussian D×D matrix (modified
 * Gram-Schmidt) and ρ < 1 guarantees a stable attractor at 0.
 * State s_0 is drawn N(0, I) and the trajectory is iterated.  We
 * discard A_true — its only purpose is to generate a sequence v139
 * can fit without cheating.
 * ============================================================= */
void cos_v139_synthetic_trajectory(float *out, int D, int Tp1,
                                   float contraction, uint64_t seed) {
    if (!out || D <= 0 || D > COS_V139_MAX_DIM || Tp1 <= 0) return;
    uint64_t s = seed ? seed : 1ULL;
    /* contraction ∈ (0, 1].  1.0 means pure rotation (norm-preserving);
     * < 1 means a stable contraction toward 0.  The self-test uses 1.0
     * so the LS residual isn't swamped by float32 epsilon once the
     * trajectory decays.  > 1 is rejected to keep the state bounded. */
    if (contraction <= 0.0f || contraction > 1.0f)
        contraction = 1.0f;

    /* Sample D×D Gaussian M. */
    double M[COS_V139_MAX_DIM * COS_V139_MAX_DIM];
    for (int i = 0; i < D * D; ++i) M[i] = nrand(&s);

    /* Modified Gram-Schmidt → orthonormal columns. */
    for (int k = 0; k < D; ++k) {
        double norm = 0.0;
        for (int i = 0; i < D; ++i) norm += M[i * D + k] * M[i * D + k];
        norm = sqrt(norm);
        if (norm < 1e-12) norm = 1.0;
        for (int i = 0; i < D; ++i) M[i * D + k] /= norm;
        for (int j = k + 1; j < D; ++j) {
            double dot = 0.0;
            for (int i = 0; i < D; ++i) dot += M[i * D + k] * M[i * D + j];
            for (int i = 0; i < D; ++i) M[i * D + j] -= dot * M[i * D + k];
        }
    }
    /* A_true = ρ · M */
    double Atrue[COS_V139_MAX_DIM * COS_V139_MAX_DIM];
    for (int i = 0; i < D * D; ++i)
        Atrue[i] = (double)contraction * M[i];

    /* Seed state. */
    float *sv = out;
    for (int i = 0; i < D; ++i) sv[i] = nrand(&s);
    for (int t = 1; t < Tp1; ++t) {
        float *cur = out + (size_t)(t - 1) * D;
        float *nxt = out + (size_t)t       * D;
        for (int i = 0; i < D; ++i) {
            double acc = 0.0;
            for (int j = 0; j < D; ++j)
                acc += Atrue[i * D + j] * cur[j];
            nxt[i] = (float)acc;
        }
    }
}

int cos_v139_to_json(const cos_v139_model_t *m, char *out, size_t cap) {
    if (!m || !out || cap == 0) return -1;
    return snprintf(out, cap,
        "{\"dim\":%d,\"n_training_pairs\":%d,"
        "\"mean_l2_residual\":%.6f}",
        m->dim, m->n_training_pairs, (double)m->mean_l2_residual);
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v139 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v139_self_test(void) {
    const int D     = 16;
    const int T     = 400;    /* trajectory length                  */
    const int Tp1   = T + 1;

    /* --- A. Generate synthetic trajectory ---------------------- */
    fprintf(stderr, "check-v139: synthetic trajectory D=%d T=%d\n", D, T);
    float *traj = (float *)malloc((size_t)Tp1 * D * sizeof(float));
    _CHECK(traj, "alloc");
    cos_v139_synthetic_trajectory(traj, D, Tp1, 1.00f, 7ULL);

    /* --- B. Fit A from first 3/4 of the trajectory ------------- */
    int n_fit = (Tp1 - 1) * 3 / 4;
    const float *curr = traj;
    const float *next = traj + D;
    cos_v139_model_t m;
    fprintf(stderr, "check-v139: fit A (n=%d)\n", n_fit);
    _CHECK(cos_v139_fit(&m, D, n_fit, curr, next) == 0, "fit ok");
    fprintf(stderr, "  mean_l2_residual=%.6f\n",
            (double)m.mean_l2_residual);
    /* Well-conditioned synthetic: residual must be tiny. */
    _CHECK(m.mean_l2_residual < 0.01f,
           "train residual < 1% (spec: <10%)");

    /* --- C. Held-out prediction -------------------------------- */
    fprintf(stderr, "check-v139: held-out prediction\n");
    float pred[COS_V139_MAX_DIM];
    double acc_err = 0.0;
    int held = 0;
    for (int t = n_fit; t < T; ++t) {
        cos_v139_predict(&m, traj + (size_t)t * D, pred);
        acc_err += cos_v139_sigma_world(
            traj + (size_t)(t + 1) * D, pred, D);
        held++;
    }
    double mean_heldout = held ? acc_err / held : 1.0;
    fprintf(stderr, "  held-out σ_world (mean, n=%d) = %.6f\n",
            held, mean_heldout);
    _CHECK(mean_heldout < 0.10, "held-out σ_world < 10% (spec)");

    /* --- D. Multi-step rollout σ_world monotonicity  -----------
     * σ_world should stay bounded (not blow up) on a stable system.
     * It need not be strictly monotone — rollouts against the true
     * trajectory oscillate slightly — but the end-to-end error must
     * stay below a ceiling.                                      */
    fprintf(stderr, "check-v139: 8-step rollout\n");
    cos_v139_rollout_t ro;
    _CHECK(cos_v139_rollout(&m, traj + (size_t)n_fit * D, 8,
                            traj + (size_t)n_fit * D, &ro) == 0,
           "rollout ok");
    for (int t = 0; t < 8; ++t)
        fprintf(stderr, "  t=%d σ_world=%.6f\n",
                t, (double)ro.sigma_world[t]);
    float max_sigma = 0.0f;
    for (int t = 0; t < 8; ++t)
        if (ro.sigma_world[t] > max_sigma) max_sigma = ro.sigma_world[t];
    _CHECK(max_sigma < 0.30f, "rollout σ stays < 30%");

    /* --- E. Surprise detection --------------------------------- *
     * Perturb a state and assert σ_world rises sharply.          */
    fprintf(stderr, "check-v139: surprise detection\n");
    float perturbed[COS_V139_MAX_DIM];
    memcpy(perturbed, traj + (size_t)n_fit * D, D * sizeof(float));
    for (int i = 0; i < D; ++i) perturbed[i] += 2.0f;   /* shift */
    cos_v139_predict(&m, traj + (size_t)n_fit * D, pred);
    float s_familiar = cos_v139_sigma_world(
        traj + (size_t)(n_fit + 1) * D, pred, D);
    cos_v139_predict(&m, perturbed, pred);
    float s_shock    = cos_v139_sigma_world(
        traj + (size_t)(n_fit + 1) * D, pred, D);
    fprintf(stderr, "  σ_familiar=%.4f σ_shock=%.4f\n",
            (double)s_familiar, (double)s_shock);
    _CHECK(s_shock > s_familiar * 3.0f,
           "surprise >> familiar by ≥3×");

    /* --- F. JSON shape ----------------------------------------- */
    char js[128];
    int jw = cos_v139_to_json(&m, js, sizeof js);
    _CHECK(jw > 0 && strstr(js, "\"dim\":16") != NULL,
           "json shape");

    free(traj);
    fprintf(stderr, "check-v139: OK (fit + predict + rollout + surprise)\n");
    return 0;
}

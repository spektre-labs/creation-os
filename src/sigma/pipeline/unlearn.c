/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Unlearn primitive. */

#include "unlearn.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#define COS_UNLEARN_EPS 1e-8f

static float clamp01(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

uint64_t cos_sigma_unlearn_hash(const char *s) {
    if (!s) return 0xcbf29ce484222325ULL;  /* FNV-1a-64 empty seed */
    uint64_t h = 0xcbf29ce484222325ULL;
    for (; *s; ++s) {
        h ^= (uint64_t)(unsigned char)*s;
        h *= 0x100000001b3ULL;
    }
    return h ? h : 1ULL;   /* avoid 0 for "no-subject" sentinel */
}

/* Projection removal: w ← w − strength · proj_t(w).  Returns the
 * L1 norm of the delta written.  Degenerate t (||t||² ≤ eps) →
 * no-op (nothing to project against → nothing to forget). */
static float apply_once(float *w, const float *t, int n, float s) {
    double dot = 0.0, tt = 0.0;
    for (int i = 0; i < n; ++i) {
        dot += (double)w[i] * (double)t[i];
        tt  += (double)t[i] * (double)t[i];
    }
    if (tt <= (double)COS_UNLEARN_EPS) return 0.0f;

    float coeff = (float)((double)s * dot / tt);
    float delta_l1 = 0.0f;
    for (int i = 0; i < n; ++i) {
        float d = coeff * t[i];
        w[i] -= d;
        delta_l1 += fabsf(d);
    }
    return delta_l1;
}

void cos_sigma_unlearn_apply(float *weights, const float *target,
                             int n, float strength)
{
    if (!weights || !target || n <= 0) return;
    if (strength <= 0.0f) return;
    (void)apply_once(weights, target, n, clamp01(strength));
}

float cos_sigma_unlearn_verify(const float *w, const float *t, int n) {
    if (!w || !t || n <= 0) return 1.0f;

    double dot = 0.0, nw = 0.0, nt = 0.0;
    for (int i = 0; i < n; ++i) {
        dot += (double)w[i] * (double)t[i];
        nw  += (double)w[i] * (double)w[i];
        nt  += (double)t[i] * (double)t[i];
    }
    if (nw <= COS_UNLEARN_EPS || nt <= COS_UNLEARN_EPS) return 1.0f;
    double cs = dot / (sqrt(nw) * sqrt(nt));
    if (cs < 0) cs = -cs;
    if (cs > 1.0) cs = 1.0;
    return clamp01(1.0f - (float)cs);
}

int cos_sigma_unlearn_iterate(float *w, const float *t, int n,
                              const cos_unlearn_request_t *req,
                              cos_unlearn_result_t *res)
{
    if (!w || !t || !req || !res || n <= 0) return -1;
    if (!(req->strength > 0.0f) || req->max_iters <= 0) return -2;
    if (!(req->sigma_target > 0.0f && req->sigma_target <= 1.0f))
        return -3;

    memset(res, 0, sizeof *res);
    res->subject_hash = req->subject_hash;
    res->sigma_before = cos_sigma_unlearn_verify(w, t, n);

    float strength = clamp01(req->strength);
    float total_l1 = 0.0f;
    int   iters    = 0;
    for (int i = 0; i < req->max_iters; ++i) {
        float sig = cos_sigma_unlearn_verify(w, t, n);
        if (sig >= req->sigma_target) break;
        total_l1 += apply_once(w, t, n, strength);
        ++iters;
    }

    res->n_iters     = iters;
    res->sigma_after = cos_sigma_unlearn_verify(w, t, n);
    res->l1_shrunk   = total_l1;
    res->succeeded   = (res->sigma_after >= req->sigma_target);
    return 0;
}

/* --- self-test --------------------------------------------------------- */

static int check_hash(void) {
    /* NULL → canonical FNV-1a-64 offset basis (matches engram). */
    if (cos_sigma_unlearn_hash(NULL) != 0xcbf29ce484222325ULL) return 10;
    /* Same-input determinism. */
    uint64_t a = cos_sigma_unlearn_hash("user_42");
    uint64_t b = cos_sigma_unlearn_hash("user_42");
    if (a != b) return 11;
    /* Different inputs → different hashes (collision-avoidance
     * smoke; not a crypto property, just a FNV correctness check). */
    if (a == cos_sigma_unlearn_hash("user_43")) return 12;
    /* Non-zero sentinel. */
    if (cos_sigma_unlearn_hash("") == 0ULL) return 13;
    return 0;
}

static int check_verify_degenerate(void) {
    float w[3] = {0.0f, 0.0f, 0.0f};
    float t[3] = {1.0f, 1.0f, 1.0f};
    if (cos_sigma_unlearn_verify(w, t, 3) != 1.0f) return 20;
    float w2[3] = {1.0f, 2.0f, 3.0f};
    float t2[3] = {0.0f, 0.0f, 0.0f};
    if (cos_sigma_unlearn_verify(w2, t2, 3) != 1.0f) return 21;
    if (cos_sigma_unlearn_verify(NULL, t, 3) != 1.0f) return 22;
    return 0;
}

static int check_apply_monotone(void) {
    /* Note: projection removal preserves direction when w is exact-
     * ly parallel to t (it just shrinks |w|), so cos and σ stay
     * fixed.  Use a slightly perturbed w to exercise the direction-
     * changing case; σ should then rise on each pass with 0<s<1. */
    float w[4] = {1.0f, 2.0f, 3.0f, 4.5f};   /* 4.5 ≠ 4 → not || */
    float t[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float s0 = cos_sigma_unlearn_verify(w, t, 4);
    cos_sigma_unlearn_apply(w, t, 4, 0.5f);
    float s1 = cos_sigma_unlearn_verify(w, t, 4);
    cos_sigma_unlearn_apply(w, t, 4, 0.5f);
    float s2 = cos_sigma_unlearn_verify(w, t, 4);
    cos_sigma_unlearn_apply(w, t, 4, 0.5f);
    float s3 = cos_sigma_unlearn_verify(w, t, 4);
    if (!(s1 > s0 && s2 > s1 && s3 > s2)) return 31;

    /* Strength = 1.0 → orthogonal in one pass (σ ≈ 1). */
    float w2[3] = {3.0f, 0.0f, 0.0f};
    float t2[3] = {1.0f, 0.0f, 0.0f};
    cos_sigma_unlearn_apply(w2, t2, 3, 1.0f);
    if (cos_sigma_unlearn_verify(w2, t2, 3) < 0.999f) return 32;

    /* Orthogonal w, t → coeff = 0 → no change, σ stays at 1. */
    float w3[3] = {0.0f, 5.0f, 0.0f};
    float t3[3] = {1.0f, 0.0f, 0.0f};
    float w3_save[3] = {0.0f, 5.0f, 0.0f};
    cos_sigma_unlearn_apply(w3, t3, 3, 0.5f);
    for (int i = 0; i < 3; ++i)
        if (w3[i] != w3_save[i]) return 33;
    return 0;
}

static int check_iterate_converges(void) {
    /* Weights have a dominant component aligned with target; a few
     * iterations should drive σ_unlearn above 0.5. */
    float w[4] = {5.0f, 0.1f, 0.1f, 0.1f};
    float t[4] = {3.0f, 0.0f, 0.0f, 0.0f};
    cos_unlearn_request_t req = {
        .subject_hash = cos_sigma_unlearn_hash("user_42"),
        .strength     = 0.5f,
        .max_iters    = 20,
        .sigma_target = 0.95f,
    };
    cos_unlearn_result_t res;
    int rc = cos_sigma_unlearn_iterate(w, t, 4, &req, &res);
    if (rc != 0) return 40;
    if (!res.succeeded) return 41;
    if (res.sigma_after < 0.95f) return 42;
    if (res.n_iters <= 0 || res.n_iters > req.max_iters) return 43;
    if (res.subject_hash == 0ULL) return 44;
    if (res.l1_shrunk <= 0.0f) return 45;
    return 0;
}

static int check_iterate_guards(void) {
    float w[2] = {1.0f, 1.0f}, t[2] = {1.0f, 0.0f};
    cos_unlearn_result_t res;
    cos_unlearn_request_t bad = {0};
    bad.strength = 0.5f; bad.max_iters = 5; bad.sigma_target = 0.9f;
    if (cos_sigma_unlearn_iterate(NULL, t, 2, &bad, &res) >= 0) return 50;
    if (cos_sigma_unlearn_iterate(w, NULL, 2, &bad, &res) >= 0) return 51;
    if (cos_sigma_unlearn_iterate(w, t, 0, &bad, &res) >= 0) return 52;
    cos_unlearn_request_t bs = bad; bs.strength = 0.0f;
    if (cos_sigma_unlearn_iterate(w, t, 2, &bs, &res) >= 0) return 53;
    cos_unlearn_request_t bm = bad; bm.max_iters = 0;
    if (cos_sigma_unlearn_iterate(w, t, 2, &bm, &res) >= 0) return 54;
    cos_unlearn_request_t bt = bad; bt.sigma_target = 1.5f;
    if (cos_sigma_unlearn_iterate(w, t, 2, &bt, &res) >= 0) return 55;
    return 0;
}

static int check_zero_strength_noop(void) {
    float w[3]  = {1.0f, 2.0f, 3.0f};
    float wcp[3];
    memcpy(wcp, w, sizeof wcp);
    float t[3]  = {0.5f, 0.5f, 0.5f};
    cos_sigma_unlearn_apply(w, t, 3, 0.0f);
    for (int i = 0; i < 3; ++i) if (w[i] != wcp[i]) return 60;
    return 0;
}

int cos_sigma_unlearn_self_test(void) {
    int rc;
    if ((rc = check_hash()))                return rc;
    if ((rc = check_verify_degenerate()))   return rc;
    if ((rc = check_apply_monotone()))      return rc;
    if ((rc = check_iterate_converges()))   return rc;
    if ((rc = check_iterate_guards()))      return rc;
    if ((rc = check_zero_strength_noop()))  return rc;
    return 0;
}

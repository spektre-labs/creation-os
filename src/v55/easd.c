/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "easd.h"
#include "sigma3.h"

#include <math.h>
#include <string.h>

#define V55_EASD_MAX_TOP 32

/* Bounded-K top-index extraction.  Writes indices in descending
 * probability order into out_idx[0..k-1]. */
static int v55_top_k_indices(const float *p, int n, int k, int *out_idx)
{
    if (k > n) k = n;
    if (k > V55_EASD_MAX_TOP) k = V55_EASD_MAX_TOP;
    float vals[V55_EASD_MAX_TOP];
    for (int i = 0; i < k; i++) { vals[i] = -1.0f; out_idx[i] = -1; }
    for (int i = 0; i < n; i++) {
        float v = p[i];
        int pos = -1;
        for (int j = 0; j < k; j++) {
            if (v > vals[j]) { pos = j; break; }
        }
        if (pos < 0) continue;
        for (int j = k - 1; j > pos; j--) {
            vals[j] = vals[j - 1];
            out_idx[j] = out_idx[j - 1];
        }
        vals[pos] = v;
        out_idx[pos] = i;
    }
    return k;
}

/* Overlap = |A ∩ B| for two index sets of equal size k.  O(k²) — k
 * is bounded small (<= 32), so a hash is overkill and cache-unfriendly. */
static int v55_index_set_overlap(const int *a, const int *b, int k)
{
    int cnt = 0;
    for (int i = 0; i < k; i++) {
        int ai = a[i];
        if (ai < 0) continue;
        for (int j = 0; j < k; j++) {
            if (b[j] == ai) { cnt++; break; }
        }
    }
    return cnt;
}

/* Normalized entropy via sigma3 helper (we avoid re-implementing). */
static float v55_entropy_norm(const float *probs, int n)
{
    v55_sigma3_t s;
    v55_sigma3_from_probs(probs, n, 8, &s);
    return s.sigma_decoding;  /* H / log2(N), in [0,1] */
}

void v55_easd_decide(const v55_easd_params_t *p,
                     const float *probs_target,
                     const float *probs_draft,
                     int n,
                     v55_easd_decision_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!p || !probs_target || !probs_draft || n <= 0) return;

    int k = p->top_n;
    if (k <= 0) k = 8;
    if (k > V55_EASD_MAX_TOP) k = V55_EASD_MAX_TOP;
    if (k > n) k = n;

    int tidx[V55_EASD_MAX_TOP];
    int didx[V55_EASD_MAX_TOP];
    v55_top_k_indices(probs_target, n, k, tidx);
    v55_top_k_indices(probs_draft,  n, k, didx);

    int overlap = v55_index_set_overlap(tidx, didx, k);
    float overlap_frac = (k > 0) ? ((float)overlap / (float)k) : 0.0f;

    float h_t = v55_entropy_norm(probs_target, n);
    float h_d = v55_entropy_norm(probs_draft,  n);

    out->h_target_norm = h_t;
    out->h_draft_norm  = h_d;
    out->overlap_frac  = overlap_frac;

    /* Reject if BOTH high-entropy AND top-N substantially overlap.
     * All compares are branchless; the AND reduces to a mask on
     * aarch64. */
    int reject = (h_t >= p->h_thresh)
              && (h_d >= p->h_thresh)
              && (overlap_frac >= p->overlap_thresh);
    out->reject = reject ? 1 : 0;
}

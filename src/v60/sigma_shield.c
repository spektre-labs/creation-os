/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v60 — σ-Shield: branchless, constant-time runtime
 * security kernel.
 *
 * Hardware discipline:
 *   - no dynamic allocation on the authorise hot path
 *   - no `if` on the decision hot path (0/1 masks, AND-NOT cascade)
 *   - constant-time hash equality (no early-exit compare)
 *   - prefetch 16 lanes ahead in the batch path
 *   - 64-byte aligned request buffers
 */

#include "sigma_shield.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Defaults                                                            */
/* ------------------------------------------------------------------ */

void cos_v60_policy_default(cos_v60_policy_t *out)
{
    if (!out) return;
    out->sigma_high       = 1.00f;
    out->alpha_dominance  = 0.65f;
    out->sticky_deny_mask = COS_V60_CAP_DLSYM
                          | COS_V60_CAP_MMAP_EXEC
                          | COS_V60_CAP_SELF_MODIFY;
    out->always_caps      = 0;
    out->enforce_integrity = 1;
    out->_pad             = 0;
}

cos_v60_version_t cos_v60_version(void)
{
    cos_v60_version_t v;
    v.major = 60;
    v.minor = 0;
    v.patch = 1;
    return v;
}

const char *cos_v60_decision_tag(uint8_t d)
{
    switch (d) {
        case COS_V60_ALLOW:          return "ALLOW";
        case COS_V60_DENY_CAP:       return "DENY_CAP";
        case COS_V60_DENY_INTENT:    return "DENY_INTENT";
        case COS_V60_DENY_TOCTOU:    return "DENY_TOCTOU";
        case COS_V60_DENY_INTEGRITY: return "DENY_INTEGRITY";
        default:                     return "?";
    }
}

/* ------------------------------------------------------------------ */
/* Hash-fold (non-cryptographic, collision-resistant for TOCTOU)      */
/* ------------------------------------------------------------------ */

/* Rotating 64-bit XOR fold.  Constant number of operations per byte,
 * no branching on data, suitable for TOCTOU equality checks.  Not a
 * MAC — do not use as a security seal. */
uint64_t cos_v60_hash_fold(const void *bytes, size_t n, uint64_t seed)
{
    const uint8_t *p = (const uint8_t *)bytes;
    uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;
    size_t i = 0;

    if (!p) return h; /* h already mixed with seed */

    while (i + 8 <= n) {
        uint64_t w;
        memcpy(&w, p + i, 8);
        h ^= w;
        h  = (h << 27) | (h >> 37);
        h *= 0xBF58476D1CE4E5B9ULL;
        i += 8;
    }
    /* Tail: zero-pad but fold unconditionally; never branch on tail len. */
    uint64_t tail = 0;
    size_t rem = n - i;
    for (size_t k = 0; k < rem; ++k) {
        tail |= (uint64_t)p[i + k] << (k * 8);
    }
    h ^= tail;
    h  = (h << 13) | (h >> 51);
    h *= 0x94D049BB133111EBULL;
    h ^= h >> 31;
    return h;
}

/* Constant-time equality on 64-bit words.  No branching.             */
int cos_v60_ct_equal64(uint64_t a, uint64_t b)
{
    uint64_t d = a ^ b;
    /* Fold to a single bit without branching. */
    d |= d >> 32;
    d |= d >> 16;
    d |= d >> 8;
    d |= d >> 4;
    d |= d >> 2;
    d |= d >> 1;
    /* d is 0 if a==b, else has bit 0 set. */
    return (int)(1 ^ (d & 1));
}

/* ------------------------------------------------------------------ */
/* Authorise — the hot path                                            */
/* ------------------------------------------------------------------ */

static inline int v60_sigma_high(const cos_v60_request_t *r, float thr)
{
    return ((r->epistemic + r->aleatoric) >= thr);
}

static inline int v60_alpha_dom(const cos_v60_request_t *r, float thr)
{
    float tot = r->epistemic + r->aleatoric + 1.0e-12f;
    return ((r->aleatoric / tot) >= thr);
}

int cos_v60_authorize(const cos_v60_request_t *req,
                      uint64_t                 holder_caps,
                      uint64_t                 code_page_hash,
                      uint64_t                 baseline_hash,
                      const cos_v60_policy_t  *p,
                      cos_v60_result_t        *out)
{
    if (!req || !p || !out) return -1;

    /* Compute every check unconditionally (constant-time posture). */

    /* 1. Integrity. */
    int integrity_ok = cos_v60_ct_equal64(code_page_hash, baseline_hash)
                     | (!p->enforce_integrity);
    /* 2. Sticky deny overlap — any sticky bits requested denied. */
    uint64_t sticky_violation = req->required_caps & p->sticky_deny_mask;
    int sticky_ok  = (sticky_violation == 0);
    /* 3. Cap subset. */
    uint64_t effective_caps = holder_caps | p->always_caps;
    uint64_t missing_caps   = req->required_caps & ~effective_caps;
    int caps_ok = (missing_caps == 0);
    /* 4. TOCTOU arg-hash equality. */
    int toctou_ok = cos_v60_ct_equal64(req->arg_hash_at_entry,
                                       req->arg_hash_at_use);
    /* 5. Intent σ. */
    int sig_high  = v60_sigma_high(req, p->sigma_high);
    int alpha_dom = v60_alpha_dom(req, p->alpha_dominance);
    int intent_ok = !(sig_high & alpha_dom);

    /* Reason-bits are independent of priority. */
    uint8_t reason = 0;
    reason |= (uint8_t)((!integrity_ok) ? COS_V60_REASON_INTEGRITY : 0);
    reason |= (uint8_t)((!sticky_ok)    ? (COS_V60_REASON_STICKY
                                          | COS_V60_REASON_CAP) : 0);
    reason |= (uint8_t)((!caps_ok)      ? COS_V60_REASON_CAP    : 0);
    reason |= (uint8_t)((!toctou_ok)    ? COS_V60_REASON_TOCTOU : 0);
    reason |= (uint8_t)((!intent_ok)    ? COS_V60_REASON_INTENT : 0);

    /* Branchless priority-encoded decision: first failing check wins.
     * Mask-cascade pattern identical to v58 / v59. */
    int m_integrity = (!integrity_ok);
    int m_sticky    = (!m_integrity) & (!sticky_ok);
    int m_cap       = (!m_integrity) & (!m_sticky)    & (!caps_ok);
    int m_toctou    = (!m_integrity) & (!m_sticky) & (!m_cap) & (!toctou_ok);
    int m_intent    = (!m_integrity) & (!m_sticky) & (!m_cap)
                    & (!m_toctou)    & (!intent_ok);
    int m_allow     = (!m_integrity) & (!m_sticky) & (!m_cap)
                    & (!m_toctou)    & (!m_intent);

    uint8_t decision = (uint8_t)(
            (COS_V60_DENY_INTEGRITY * m_integrity)
          | (COS_V60_DENY_CAP       * (m_sticky | m_cap))
          | (COS_V60_DENY_TOCTOU    * m_toctou)
          | (COS_V60_DENY_INTENT    * m_intent)
          | (COS_V60_ALLOW          * m_allow));

    float sigma_tot   = req->epistemic + req->aleatoric;
    float sigma_denom = sigma_tot + 1.0e-12f;
    float alpha_frac  = req->aleatoric / sigma_denom;

    out->action_id      = req->action_id;
    out->missing_caps   = missing_caps | sticky_violation;
    out->decision       = decision;
    out->reason_bits    = reason;
    out->sigma_total    = sigma_tot;
    out->alpha_fraction = alpha_frac;
    memset(out->_pad, 0, sizeof(out->_pad));
    return 0;
}

void cos_v60_authorize_batch(const cos_v60_request_t *reqs,
                             int32_t                  n,
                             uint64_t                 holder_caps,
                             uint64_t                 code_page_hash,
                             uint64_t                 baseline_hash,
                             const cos_v60_policy_t  *p,
                             cos_v60_result_t        *results_out)
{
    if (!reqs || !p || !results_out || n <= 0) return;
    for (int32_t i = 0; i < n; ++i) {
        if (i + 16 < n) {
            __builtin_prefetch(&reqs[i + 16], 0, 3);
            __builtin_prefetch(&results_out[i + 16], 1, 3);
        }
        cos_v60_authorize(&reqs[i], holder_caps,
                          code_page_hash, baseline_hash, p,
                          &results_out[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Aligned allocator                                                   */
/* ------------------------------------------------------------------ */

cos_v60_request_t *cos_v60_alloc_requests(int32_t n)
{
    if (n <= 0) return NULL;
    size_t bytes = (size_t)n * sizeof(cos_v60_request_t);
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    void *m = aligned_alloc(64, aligned);
    if (!m) return NULL;
    memset(m, 0, aligned);
    return (cos_v60_request_t *)m;
}

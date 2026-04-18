/*
 * v170 σ-Transfer — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "transfer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float clampf(float x, float lo, float hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void set_domain(cos_v170_domain_t *d,
                       const char *name,
                       float e0, float e1, float e2, float e3,
                       float sigma) {
    memset(d, 0, sizeof(*d));
    size_t n = strlen(name);
    if (n >= sizeof(d->name)) n = sizeof(d->name) - 1;
    memcpy(d->name, name, n); d->name[n] = '\0';
    d->embed[0] = e0; d->embed[1] = e1;
    d->embed[2] = e2; d->embed[3] = e3;
    d->sigma    = clampf(sigma, 0.0f, 1.0f);
}

void cos_v170_init(cos_v170_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed             = seed ? seed : 0x170CDEFABCDE0170ULL;
    s->sigma_gap_delta  = 0.10f;   /* source must be ≥10% better */
    s->d_max            = 0.80f;   /* embedding distance ceiling */
    s->tau_src          = 0.35f;   /* source must be "strong" */

    /* Deterministic 4-D embeddings, hand-picked so that:
     *   math ↔ physics near (~0.15)
     *   math ↔ code near    (~0.30)
     *   chemistry ↔ biology near (~0.20)
     *   math ↔ poetry far   (~1.25)
     *   chemistry → biology is the canonical helpful transfer
     *   math       → poetry  is the canonical negative transfer
     */
    set_domain(&s->domains[0], "math",      0.90f, 0.80f, 0.10f, 0.10f, 0.20f);
    set_domain(&s->domains[1], "physics",   0.85f, 0.70f, 0.15f, 0.20f, 0.30f);
    set_domain(&s->domains[2], "chemistry", 0.40f, 0.30f, 0.75f, 0.10f, 0.23f);
    set_domain(&s->domains[3], "biology",   0.35f, 0.25f, 0.80f, 0.15f, 0.71f);
    set_domain(&s->domains[4], "code",      0.70f, 0.55f, 0.30f, 0.30f, 0.18f);
    set_domain(&s->domains[5], "logic",     0.78f, 0.72f, 0.25f, 0.18f, 0.25f);
    set_domain(&s->domains[6], "history",   0.10f, 0.15f, 0.20f, 0.70f, 0.55f);
    set_domain(&s->domains[7], "poetry",    0.05f, 0.08f, 0.10f, 0.85f, 0.60f);
}

int cos_v170_find_domain(const cos_v170_state_t *s, const char *name) {
    if (!s || !name) return -1;
    for (int i = 0; i < COS_V170_N_DOMAINS; ++i)
        if (strcmp(s->domains[i].name, name) == 0) return i;
    return -1;
}

float cos_v170_distance(const cos_v170_state_t *s, int a, int b) {
    if (a < 0 || b < 0 || a >= COS_V170_N_DOMAINS ||
        b >= COS_V170_N_DOMAINS) return INFINITY;
    const float *ea = s->domains[a].embed;
    const float *eb = s->domains[b].embed;
    float acc = 0.0f;
    for (int k = 0; k < COS_V170_EMBED_DIM; ++k) {
        float d = ea[k] - eb[k];
        acc += d * d;
    }
    return sqrtf(acc);
}

/* ------------------------------------------------------------- */
/* Decision                                                      */
/* ------------------------------------------------------------- */

cos_v170_decision_t
cos_v170_decide(const cos_v170_state_t *s, int target_idx) {
    cos_v170_decision_t dec;
    memset(&dec, 0, sizeof(dec));
    dec.target_idx  = target_idx;
    dec.source_idx  = -1;
    dec.d_max       = s->d_max;
    dec.tau_src     = s->tau_src;
    dec.sigma_target = (target_idx >= 0 && target_idx < COS_V170_N_DOMAINS)
                           ? s->domains[target_idx].sigma : 1.0f;
    dec.sigma_source = 1.0f;
    dec.distance    = INFINITY;

    if (target_idx < 0 || target_idx >= COS_V170_N_DOMAINS) {
        snprintf(dec.reason, sizeof(dec.reason), "invalid target");
        return dec;
    }

    /* pick closest source with σ_source ≤ τ_src AND σ_source
     * ≤ σ_target − δ; ties broken by lower σ_source. */
    int    best = -1;
    float  best_d = INFINITY;
    float  best_sig = 1.0f;
    for (int i = 0; i < COS_V170_N_DOMAINS; ++i) {
        if (i == target_idx) continue;
        float sig = s->domains[i].sigma;
        if (sig > s->tau_src) continue;
        if (sig > dec.sigma_target - s->sigma_gap_delta) continue;
        float d = cos_v170_distance(s, i, target_idx);
        if (d < best_d || (d == best_d && sig < best_sig)) {
            best = i; best_d = d; best_sig = sig;
        }
    }
    if (best < 0) {
        dec.go = false;
        snprintf(dec.reason, sizeof(dec.reason),
                 "no source: none below τ_src with σ gap ≥ δ");
        return dec;
    }
    dec.source_idx   = best;
    dec.distance     = best_d;
    dec.sigma_source = s->domains[best].sigma;

    if (dec.distance > s->d_max) {
        dec.go = false;
        snprintf(dec.reason, sizeof(dec.reason),
                 "distance %.3f > d_max %.3f (negative transfer risk)",
                 (double)dec.distance, (double)s->d_max);
        return dec;
    }
    dec.go = true;
    snprintf(dec.reason, sizeof(dec.reason),
             "source=%s σ=%.3f d=%.3f",
             s->domains[best].name,
             (double)dec.sigma_source, (double)dec.distance);
    return dec;
}

/* ------------------------------------------------------------- */
/* Application + outcome model                                   */
/* ------------------------------------------------------------- */

/* Deterministic σ_delta: the nearer the source in embedding
 * space and the larger the σ gap, the more σ drops.  If
 * distance > d_max the model *hurts* the target (negative
 * transfer), yielding a positive delta. */
static float estimated_delta(const cos_v170_state_t *s,
                             int src, int tgt) {
    float d   = cos_v170_distance(s, src, tgt);
    float gap = s->domains[tgt].sigma - s->domains[src].sigma;
    /* base improvement = α · gap · exp(−d); penalty when d > d_max */
    float alpha = 0.55f;
    float imp   = alpha * gap * expf(-d);
    float penalty = 0.0f;
    if (d > s->d_max) penalty = 0.30f * (d - s->d_max);
    /* transfer moves σ DOWN by imp and UP by penalty */
    return -imp + penalty;
}

cos_v170_outcome_t
cos_v170_apply(cos_v170_state_t *s, cos_v170_decision_t d) {
    cos_v170_outcome_t out;
    memset(&out, 0, sizeof(out));
    out.source_idx = d.source_idx;
    out.target_idx = d.target_idx;

    if (!d.go || d.target_idx < 0 || d.source_idx < 0) {
        out.sigma_before = (d.target_idx >= 0)
                               ? s->domains[d.target_idx].sigma : 1.0f;
        out.sigma_after  = out.sigma_before;
        out.delta        = 0.0f;
        out.rollback     = false;
        out.sigma_rollback = out.sigma_before;
        snprintf(out.note, sizeof(out.note),
                 "no-op: transfer not allowed");
        return out;
    }

    float before = s->domains[d.target_idx].sigma;
    float delta  = estimated_delta(s, d.source_idx, d.target_idx);
    float after  = clampf(before + delta, 0.0f, 1.0f);

    out.sigma_before = before;
    out.sigma_after  = after;
    out.delta        = delta;
    out.rollback     = (delta > 0.0f);   /* σ rose → bad */

    if (out.rollback) {
        /* v124 rollback: restore σ and record the lesson. */
        out.sigma_rollback = before;
        s->domains[d.target_idx].sigma = before;
        snprintf(out.note, sizeof(out.note),
                 "ROLLBACK: transfer %s→%s raised σ by %.3f",
                 s->domains[d.source_idx].name,
                 s->domains[d.target_idx].name,
                 (double)delta);
    } else {
        out.sigma_rollback = after;
        s->domains[d.target_idx].sigma = after;
        snprintf(out.note, sizeof(out.note),
                 "ok: transfer %s→%s reduced σ by %.3f",
                 s->domains[d.source_idx].name,
                 s->domains[d.target_idx].name,
                 (double)(-delta));
    }
    return out;
}

/* ------------------------------------------------------------- */
/* Zero-shot                                                     */
/* ------------------------------------------------------------- */

float cos_v170_zero_shot(const cos_v170_state_t *s,
                         int target_idx, int *out_k) {
    if (out_k) *out_k = 0;
    if (target_idx < 0 || target_idx >= COS_V170_N_DOMAINS) return 1.0f;

    /* Pick k=2 nearest strong neighbours. */
    int    pick[2]  = {-1, -1};
    float  pick_d[2] = {INFINITY, INFINITY};
    for (int i = 0; i < COS_V170_N_DOMAINS; ++i) {
        if (i == target_idx) continue;
        if (s->domains[i].sigma > s->tau_src) continue;
        float d = cos_v170_distance(s, i, target_idx);
        if (d < pick_d[0]) {
            pick[1] = pick[0]; pick_d[1] = pick_d[0];
            pick[0] = i;       pick_d[0] = d;
        } else if (d < pick_d[1]) {
            pick[1] = i;       pick_d[1] = d;
        }
    }
    int k = (pick[0] >= 0) + (pick[1] >= 0);
    if (out_k) *out_k = k;
    if (k == 0) return 1.0f;

    float num = 0.0f, den = 0.0f;
    for (int i = 0; i < k; ++i) {
        float d = pick_d[i];
        float w = 1.0f / (d + 1e-3f);
        num += w * s->domains[pick[i]].sigma;
        den += w;
    }
    return clampf(num / den + 0.05f, 0.0f, 1.0f);   /* small init penalty */
}

/* ------------------------------------------------------------- */
/* JSON                                                          */
/* ------------------------------------------------------------- */

size_t cos_v170_state_json(const cos_v170_state_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
                     "{\"kernel\":\"v170\",\"tau_src\":%.4f,"
                     "\"d_max\":%.4f,\"sigma_gap_delta\":%.4f,"
                     "\"domains\":[",
                     (double)s->tau_src, (double)s->d_max,
                     (double)s->sigma_gap_delta);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    for (int i = 0; i < COS_V170_N_DOMAINS; ++i) {
        const cos_v170_domain_t *d = &s->domains[i];
        n = snprintf(buf + used, cap - used,
            "%s{\"name\":\"%s\",\"sigma\":%.4f,"
            "\"embed\":[%.4f,%.4f,%.4f,%.4f]}",
            i == 0 ? "" : ",", d->name, (double)d->sigma,
            (double)d->embed[0], (double)d->embed[1],
            (double)d->embed[2], (double)d->embed[3]);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    n = snprintf(buf + used, cap - used, "]}");
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

size_t cos_v170_report_json(cos_v170_state_t *s,
                            const char *target_name,
                            char *buf, size_t cap) {
    if (!s || !buf) return 0;
    int tgt = cos_v170_find_domain(s, target_name);
    if (tgt < 0) return 0;
    cos_v170_decision_t dec = cos_v170_decide(s, tgt);
    cos_v170_outcome_t  out = cos_v170_apply(s, dec);
    const char *src_name = (dec.source_idx >= 0)
                               ? s->domains[dec.source_idx].name : "null";
    size_t used = 0;
    int n = snprintf(buf + used, cap - used,
        "{\"kernel\":\"v170\",\"event\":\"transfer\","
        "\"target\":\"%s\",\"source\":%s%s%s,"
        "\"go\":%s,\"distance\":%.4f,"
        "\"sigma_source\":%.4f,\"sigma_target\":%.4f,"
        "\"sigma_before\":%.4f,\"sigma_after\":%.4f,"
        "\"delta\":%.4f,\"rollback\":%s,"
        "\"sigma_rollback\":%.4f,"
        "\"reason\":\"%s\",\"note\":\"%s\"}",
        target_name,
        dec.source_idx >= 0 ? "\"" : "",
        src_name,
        dec.source_idx >= 0 ? "\"" : "",
        dec.go ? "true" : "false",
        (double)dec.distance,
        (double)dec.sigma_source, (double)dec.sigma_target,
        (double)out.sigma_before, (double)out.sigma_after,
        (double)out.delta,
        out.rollback ? "true" : "false",
        (double)out.sigma_rollback,
        dec.reason, out.note);
    if (n < 0 || (size_t)n >= cap - used) return 0;
    used += (size_t)n;
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v170_self_test(void) {
    cos_v170_state_t s;
    cos_v170_init(&s, 0x1700ACAB0170ULL);

    int math      = cos_v170_find_domain(&s, "math");
    int physics   = cos_v170_find_domain(&s, "physics");
    int poetry    = cos_v170_find_domain(&s, "poetry");
    int chemistry = cos_v170_find_domain(&s, "chemistry");
    int biology   = cos_v170_find_domain(&s, "biology");
    if (math < 0 || physics < 0 || poetry < 0 ||
        chemistry < 0 || biology < 0) return 1;

    float d_mp = cos_v170_distance(&s, math, physics);
    float d_mo = cos_v170_distance(&s, math, poetry);
    if (!(d_mp < d_mo)) return 2;     /* near closer than far */

    /* Decision for biology: source should be chemistry (nearest
     * strong neighbour).  Apply should drop σ and NOT rollback. */
    cos_v170_decision_t dec = cos_v170_decide(&s, biology);
    if (!dec.go) return 3;
    if (dec.source_idx != chemistry) return 4;

    cos_v170_outcome_t out = cos_v170_apply(&s, dec);
    if (out.rollback) return 5;
    if (!(out.sigma_after < out.sigma_before)) return 6;

    /* Force a negative transfer: raise poetry.σ above all
     * δ-gaps, flip math.sigma to stay low, then *manually*
     * trigger a transfer by constructing a decision that
     * violates d_max to verify rollback activates. */
    cos_v170_state_t s2;
    cos_v170_init(&s2, 0);
    cos_v170_decision_t bad;
    memset(&bad, 0, sizeof(bad));
    bad.go = true;
    bad.source_idx = math;
    bad.target_idx = poetry;
    bad.distance   = cos_v170_distance(&s2, math, poetry);
    bad.sigma_source = s2.domains[math].sigma;
    bad.sigma_target = s2.domains[poetry].sigma;
    cos_v170_outcome_t bad_out = cos_v170_apply(&s2, bad);
    if (!bad_out.rollback) return 7;
    if (!(bad_out.delta > 0.0f)) return 8;
    /* rollback should preserve original σ on target */
    if (s2.domains[poetry].sigma != bad_out.sigma_before) return 9;

    /* Zero-shot: mark physics as unseen (σ=1.0) and check
     * ensemble returns a reasonable σ. */
    cos_v170_state_t s3;
    cos_v170_init(&s3, 0);
    s3.domains[physics].sigma = 1.0f;
    int k = 0;
    float zs = cos_v170_zero_shot(&s3, physics, &k);
    if (k < 1) return 10;
    if (!(zs > 0.0f && zs < 1.0f)) return 11;

    /* Determinism */
    cos_v170_state_t a, b;
    cos_v170_init(&a, 0x1700ACAB0170ULL);
    cos_v170_init(&b, 0x1700ACAB0170ULL);
    char A[4096], B[4096];
    size_t na = cos_v170_state_json(&a, A, sizeof(A));
    size_t nb = cos_v170_state_json(&b, B, sizeof(B));
    if (na == 0 || na != nb) return 12;
    if (memcmp(A, B, na) != 0) return 13;
    return 0;
}

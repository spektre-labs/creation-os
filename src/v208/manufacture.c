/*
 * v208 σ-Manufacture — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "manufacture.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v208_init(cos_v208_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x208A1FADULL;
    s->tau_quality = 0.40f;
    s->tau_dfm     = 0.50f;
}

typedef struct {
    int design_id;
    int n_parts;
    float dfm_sigma[COS_V208_N_DFM_RULES];
    float proc_sigma[COS_V208_N_STAGES];
    int  feedback_id;
} fx_t;

/* Fixture: 4 Pareto designs from v207, each with distinct
 * DFM/process profiles.  At least one run flags each class
 * of DFM issue; σ_quality derived from the inputs below. */
static const fx_t FX[COS_V208_N_DESIGNS] = {
    /* did parts  dfm: tol   mat   asm   pc    spec    proc stages       fb */
    {   0,  24,  { 0.20f, 0.25f, 0.15f, 0.22f, 0.10f },
                 { 0.10f, 0.12f, 0.14f, 0.18f },              31 },
    {   1,  18,  { 0.55f, 0.30f, 0.28f, 0.20f, 0.40f },            /* TOL flagged */
                 { 0.12f, 0.15f, 0.22f, 0.28f },              32 },
    {   2,  32,  { 0.30f, 0.60f, 0.58f, 0.55f, 0.25f },            /* mat+asm+pc flagged */
                 { 0.20f, 0.28f, 0.35f, 0.42f },              33 },
    {   3,  40,  { 0.62f, 0.45f, 0.40f, 0.70f, 0.65f },            /* tol+pc+spec flagged */
                 { 0.30f, 0.38f, 0.45f, 0.52f },              34 }
};

void cos_v208_build(cos_v208_state_t *s) {
    s->n = COS_V208_N_DESIGNS;
    for (int i = 0; i < s->n; ++i) {
        cos_v208_run_t *r = &s->runs[i];
        memset(r, 0, sizeof(*r));
        r->id        = i;
        r->design_id = FX[i].design_id;
        r->n_parts   = FX[i].n_parts;
        r->n_stages  = COS_V208_N_STAGES;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k) {
            r->dfm[k].rule      = k;
            r->dfm[k].sigma_dfm = FX[i].dfm_sigma[k];
        }
        for (int k = 0; k < COS_V208_N_STAGES; ++k)
            r->sigma_process[k] = FX[i].proc_sigma[k];
        r->feedback_hypothesis_id = FX[i].feedback_id;
    }
}

void cos_v208_run(cos_v208_state_t *s) {
    s->total_dfm_flagged = 0;
    s->total_checkpoints = 0;

    uint64_t prev = 0x20812E85ULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v208_run_t *r = &s->runs[i];

        /* DFM flagging + suggestion */
        r->n_dfm_flagged = 0;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k) {
            cos_v208_dfm_finding_t *f = &r->dfm[k];
            f->flagged = f->sigma_dfm > s->tau_dfm;
            f->suggestion_id = f->flagged ? (100 + i * 10 + k) : 0;
            if (f->flagged) r->n_dfm_flagged++;
        }
        s->total_dfm_flagged += r->n_dfm_flagged;

        /* Process sim max. */
        float pmax = 0.0f;
        for (int k = 0; k < COS_V208_N_STAGES; ++k)
            if (r->sigma_process[k] > pmax) pmax = r->sigma_process[k];
        r->sigma_process_max = pmax;

        /* Quality prediction: monotone in (process max, dfm flags). */
        float dfm_mean = 0.0f;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k)
            dfm_mean += r->dfm[k].sigma_dfm;
        dfm_mean /= (float)COS_V208_N_DFM_RULES;
        float q = 0.6f * pmax + 0.4f * dfm_mean;
        if (q < 0.0f) q = 0.0f;
        if (q > 1.0f) q = 1.0f;
        r->sigma_quality = q;

        /* v159 heal: extra checkpoints when σ_quality high. */
        int base_cp = 2;
        int extra   = 0;
        if (q > s->tau_quality) {
            float over = (q - s->tau_quality) / (1.0f - s->tau_quality);
            extra = (int)(over * 6.0f + 0.5f);
        }
        r->n_checkpoints = base_cp + extra;
        s->total_checkpoints += r->n_checkpoints;

        /* Hash record. */
        r->hash_prev = prev;
        struct {
            int id, design_id, n_parts, n_flagged, n_cp, fb;
            float pmax, q;
            float dfm[COS_V208_N_DFM_RULES];
            float ps[COS_V208_N_STAGES];
            uint64_t prev;
        } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id        = r->id;
        rec.design_id = r->design_id;
        rec.n_parts   = r->n_parts;
        rec.n_flagged = r->n_dfm_flagged;
        rec.n_cp      = r->n_checkpoints;
        rec.fb        = r->feedback_hypothesis_id;
        rec.pmax      = r->sigma_process_max;
        rec.q         = r->sigma_quality;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k)
            rec.dfm[k] = r->dfm[k].sigma_dfm;
        for (int k = 0; k < COS_V208_N_STAGES; ++k)
            rec.ps[k]  = r->sigma_process[k];
        rec.prev      = prev;
        r->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = r->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x20812E85ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v208_run_t *r = &s->runs[i];
        if (r->hash_prev != v) { s->chain_valid = false; break; }
        struct {
            int id, design_id, n_parts, n_flagged, n_cp, fb;
            float pmax, q;
            float dfm[COS_V208_N_DFM_RULES];
            float ps[COS_V208_N_STAGES];
            uint64_t prev;
        } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id        = r->id;
        rec.design_id = r->design_id;
        rec.n_parts   = r->n_parts;
        rec.n_flagged = r->n_dfm_flagged;
        rec.n_cp      = r->n_checkpoints;
        rec.fb        = r->feedback_hypothesis_id;
        rec.pmax      = r->sigma_process_max;
        rec.q         = r->sigma_quality;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k)
            rec.dfm[k] = r->dfm[k].sigma_dfm;
        for (int k = 0; k < COS_V208_N_STAGES; ++k)
            rec.ps[k]  = r->sigma_process[k];
        rec.prev      = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != r->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v208_to_json(const cos_v208_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v208\","
        "\"n\":%d,\"tau_quality\":%.3f,\"tau_dfm\":%.3f,"
        "\"total_dfm_flagged\":%d,\"total_checkpoints\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"runs\":[",
        s->n, s->tau_quality, s->tau_dfm,
        s->total_dfm_flagged, s->total_checkpoints,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v208_run_t *r = &s->runs[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"design\":%d,\"parts\":%d,"
            "\"n_flagged\":%d,\"pmax\":%.3f,\"q\":%.3f,"
            "\"cp\":%d,\"fb\":%d,\"dfm\":[",
            i == 0 ? "" : ",", r->id, r->design_id, r->n_parts,
            r->n_dfm_flagged, r->sigma_process_max,
            r->sigma_quality, r->n_checkpoints,
            r->feedback_hypothesis_id);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
        for (int kk = 0; kk < COS_V208_N_DFM_RULES; ++kk) {
            int jj = snprintf(buf + off, cap - off,
                "%s{\"rule\":%d,\"s\":%.3f,\"flag\":%s,\"sug\":%d}",
                kk == 0 ? "" : ",", r->dfm[kk].rule,
                r->dfm[kk].sigma_dfm,
                r->dfm[kk].flagged ? "true" : "false",
                r->dfm[kk].suggestion_id);
            if (jj < 0 || off + (size_t)jj >= cap) return 0;
            off += (size_t)jj;
        }
        int j2 = snprintf(buf + off, cap - off, "]}");
        if (j2 < 0 || off + (size_t)j2 >= cap) return 0;
        off += (size_t)j2;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v208_self_test(void) {
    cos_v208_state_t s;
    cos_v208_init(&s, 0x208A1FADULL);
    cos_v208_build(&s);
    cos_v208_run(&s);

    if (s.n != COS_V208_N_DESIGNS) return 1;
    if (s.total_dfm_flagged < 1)   return 2;
    if (!s.chain_valid)             return 3;

    for (int i = 0; i < s.n; ++i) {
        const cos_v208_run_t *r = &s.runs[i];
        if (r->sigma_quality < 0.0f || r->sigma_quality > 1.0f) return 4;
        if (r->sigma_process_max < 0.0f || r->sigma_process_max > 1.0f) return 4;
        for (int k = 0; k < COS_V208_N_DFM_RULES; ++k)
            if (r->dfm[k].sigma_dfm < 0.0f || r->dfm[k].sigma_dfm > 1.0f) return 4;
        if (r->feedback_hypothesis_id == 0) return 5;   /* closed loop */
        if (r->n_checkpoints < 2)           return 6;
    }

    /* Monotone check: higher σ_quality ⇒ ≥ checkpoints. */
    for (int i = 0; i < s.n; ++i) {
        for (int j = 0; j < s.n; ++j) {
            const cos_v208_run_t *a = &s.runs[i];
            const cos_v208_run_t *b = &s.runs[j];
            if (a->sigma_quality >= b->sigma_quality - 1e-6f) {
                if (a->n_checkpoints < b->n_checkpoints) return 7;
            }
        }
    }
    return 0;
}

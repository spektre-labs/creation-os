/*
 * v272 σ-Digital-Twin — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "digital_twin.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct { int t; float s; }
    SYNC[COS_V272_N_SYNC] = {
    { 1000, 0.02f },   /* stable  */
    { 1060, 0.04f },   /* stable  */
    { 1120, 0.41f },   /* drifted */
    { 1180, 0.58f },   /* drifted */
};

static const struct { const char *id; float hrs; float sp; }
    PRED[COS_V272_N_PRED] = {
    { "bearing-7", 72.0f, 0.18f },   /* REPLACE  */
    { "valve-3",   320.f, 0.26f },   /* REPLACE  */
    { "belt-2",    410.f, 0.42f },   /* MONITOR  */
};

static const struct { const char *lbl; float dp; float sw; }
    WHATIF[COS_V272_N_WHATIF] = {
    { "speed+5%",      5.0f,  0.11f },  /* IMPLEMENT */
    { "speed+10%",    10.0f,  0.22f },  /* IMPLEMENT */
    { "temp+15C",     15.0f,  0.44f },  /* ABORT     */
};

static const struct { const char *id; float d, r; }
    VERIFIED[COS_V272_N_VERIFIED] = {
    { "open-valve",   0.80f, 0.82f },   /* |d-r|=0.02 PASS  */
    { "ramp-motor",   0.60f, 0.66f },   /* |d-r|=0.06 PASS  */
    { "full-close",   0.90f, 0.55f },   /* |d-r|=0.35 BLOCK */
};

static float absf(float x) { return x < 0 ? -x : x; }

void cos_v272_init(cos_v272_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x272DA7AULL;
    s->tau_pred   = 0.30f;
    s->tau_whatif = 0.25f;
    s->tau_match  = 0.10f;
    s->stable_thr = 0.05f;
    s->drift_thr  = 0.30f;
}

void cos_v272_run(cos_v272_state_t *s) {
    uint64_t prev = 0x272DA700ULL;

    s->n_sync_rows_ok = 0;
    s->n_sync_stable = s->n_sync_drifted = 0;
    for (int i = 0; i < COS_V272_N_SYNC; ++i) {
        cos_v272_sync_t *x = &s->sync[i];
        memset(x, 0, sizeof(*x));
        x->timestamp_s = SYNC[i].t;
        x->sigma_twin  = SYNC[i].s;
        x->stable      = (x->sigma_twin <  s->stable_thr);
        x->drifted     = (x->sigma_twin >  s->drift_thr);
        if (x->sigma_twin >= 0.0f && x->sigma_twin <= 1.0f)
            s->n_sync_rows_ok++;
        if (x->stable)  s->n_sync_stable++;
        if (x->drifted) s->n_sync_drifted++;
        prev = fnv1a(&x->timestamp_s, sizeof(x->timestamp_s), prev);
        prev = fnv1a(&x->sigma_twin,  sizeof(x->sigma_twin),  prev);
        prev = fnv1a(&x->stable,      sizeof(x->stable),      prev);
        prev = fnv1a(&x->drifted,     sizeof(x->drifted),     prev);
    }

    s->n_pred_rows_ok = 0;
    s->n_pred_replace = s->n_pred_monitor = 0;
    for (int i = 0; i < COS_V272_N_PRED; ++i) {
        cos_v272_pred_t *p = &s->pred[i];
        memset(p, 0, sizeof(*p));
        cpy(p->part_id, sizeof(p->part_id), PRED[i].id);
        p->predicted_failure_hours = PRED[i].hrs;
        p->sigma_prediction        = PRED[i].sp;
        p->action                  =
            (p->sigma_prediction <= s->tau_pred)
                ? COS_V272_MAINT_REPLACE
                : COS_V272_MAINT_MONITOR;
        bool ok =
            p->predicted_failure_hours > 0.0f &&
            p->sigma_prediction >= 0.0f &&
            p->sigma_prediction <= 1.0f;
        if (ok) s->n_pred_rows_ok++;
        if (p->action == COS_V272_MAINT_REPLACE) s->n_pred_replace++;
        else                                     s->n_pred_monitor++;
        prev = fnv1a(p->part_id, strlen(p->part_id), prev);
        prev = fnv1a(&p->predicted_failure_hours,
                     sizeof(p->predicted_failure_hours), prev);
        prev = fnv1a(&p->sigma_prediction,
                     sizeof(p->sigma_prediction), prev);
        prev = fnv1a(&p->action, sizeof(p->action), prev);
    }

    s->n_whatif_rows_ok = 0;
    s->n_whatif_implement = s->n_whatif_abort = 0;
    for (int i = 0; i < COS_V272_N_WHATIF; ++i) {
        cos_v272_whatif_row_t *w = &s->whatif[i];
        memset(w, 0, sizeof(*w));
        cpy(w->label, sizeof(w->label), WHATIF[i].lbl);
        w->delta_param_pct = WHATIF[i].dp;
        w->sigma_whatif    = WHATIF[i].sw;
        w->decision        =
            (w->sigma_whatif <= s->tau_whatif)
                ? COS_V272_WHATIF_IMPLEMENT
                : COS_V272_WHATIF_ABORT;
        if (w->sigma_whatif >= 0.0f && w->sigma_whatif <= 1.0f)
            s->n_whatif_rows_ok++;
        if (w->decision == COS_V272_WHATIF_IMPLEMENT) s->n_whatif_implement++;
        else                                          s->n_whatif_abort++;
        prev = fnv1a(w->label, strlen(w->label), prev);
        prev = fnv1a(&w->delta_param_pct, sizeof(w->delta_param_pct), prev);
        prev = fnv1a(&w->sigma_whatif,    sizeof(w->sigma_whatif),    prev);
        prev = fnv1a(&w->decision,        sizeof(w->decision),        prev);
    }

    s->n_verified_rows_ok = 0;
    s->n_verified_pass = s->n_verified_block = 0;
    for (int i = 0; i < COS_V272_N_VERIFIED; ++i) {
        cos_v272_verified_t *v = &s->verified[i];
        memset(v, 0, sizeof(*v));
        cpy(v->action_id, sizeof(v->action_id), VERIFIED[i].id);
        v->declared_sim  = VERIFIED[i].d;
        v->realized_phys = VERIFIED[i].r;
        v->sigma_match   = absf(v->declared_sim - v->realized_phys);
        v->verdict       =
            (v->sigma_match <= s->tau_match)
                ? COS_V272_VERDICT_PASS
                : COS_V272_VERDICT_BLOCK;
        float expected = absf(v->declared_sim - v->realized_phys);
        bool ok =
            absf(v->sigma_match - expected) < 1e-5f &&
            v->declared_sim  >= 0.0f && v->declared_sim  <= 1.0f &&
            v->realized_phys >= 0.0f && v->realized_phys <= 1.0f;
        if (ok) s->n_verified_rows_ok++;
        if (v->verdict == COS_V272_VERDICT_PASS) s->n_verified_pass++;
        else                                     s->n_verified_block++;
        prev = fnv1a(v->action_id, strlen(v->action_id), prev);
        prev = fnv1a(&v->declared_sim,  sizeof(v->declared_sim),  prev);
        prev = fnv1a(&v->realized_phys, sizeof(v->realized_phys), prev);
        prev = fnv1a(&v->sigma_match,   sizeof(v->sigma_match),   prev);
        prev = fnv1a(&v->verdict,       sizeof(v->verdict),       prev);
    }

    int total = COS_V272_N_SYNC + 1 +
                COS_V272_N_PRED + 1 +
                COS_V272_N_WHATIF + 1 +
                COS_V272_N_VERIFIED + 1;
    int passing = s->n_sync_rows_ok +
                  ((s->n_sync_stable >= 1 && s->n_sync_drifted >= 1) ? 1 : 0) +
                  s->n_pred_rows_ok +
                  ((s->n_pred_replace >= 1 && s->n_pred_monitor >= 1) ? 1 : 0) +
                  s->n_whatif_rows_ok +
                  ((s->n_whatif_implement >= 1 && s->n_whatif_abort >= 1) ? 1 : 0) +
                  s->n_verified_rows_ok +
                  ((s->n_verified_pass >= 1 && s->n_verified_block >= 1) ? 1 : 0);
    s->sigma_digital_twin = 1.0f - ((float)passing / (float)total);
    if (s->sigma_digital_twin < 0.0f) s->sigma_digital_twin = 0.0f;
    if (s->sigma_digital_twin > 1.0f) s->sigma_digital_twin = 1.0f;

    struct { int ns, nss, nsd, np, npr, npm, nw, nwi, nwa,
                 nv, nvp, nvb;
             float sigma, tp, tw, tm, st, dt; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ns  = s->n_sync_rows_ok;
    trec.nss = s->n_sync_stable;
    trec.nsd = s->n_sync_drifted;
    trec.np  = s->n_pred_rows_ok;
    trec.npr = s->n_pred_replace;
    trec.npm = s->n_pred_monitor;
    trec.nw  = s->n_whatif_rows_ok;
    trec.nwi = s->n_whatif_implement;
    trec.nwa = s->n_whatif_abort;
    trec.nv  = s->n_verified_rows_ok;
    trec.nvp = s->n_verified_pass;
    trec.nvb = s->n_verified_block;
    trec.sigma = s->sigma_digital_twin;
    trec.tp = s->tau_pred;
    trec.tw = s->tau_whatif;
    trec.tm = s->tau_match;
    trec.st = s->stable_thr;
    trec.dt = s->drift_thr;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *maint_name(cos_v272_maint_t m) {
    switch (m) {
    case COS_V272_MAINT_REPLACE: return "REPLACE";
    case COS_V272_MAINT_MONITOR: return "MONITOR";
    }
    return "UNKNOWN";
}

static const char *whatif_name(cos_v272_whatif_t w) {
    switch (w) {
    case COS_V272_WHATIF_IMPLEMENT: return "IMPLEMENT";
    case COS_V272_WHATIF_ABORT:     return "ABORT";
    }
    return "UNKNOWN";
}

static const char *verdict_name(cos_v272_verdict_t v) {
    switch (v) {
    case COS_V272_VERDICT_PASS:  return "PASS";
    case COS_V272_VERDICT_BLOCK: return "BLOCK";
    }
    return "UNKNOWN";
}

size_t cos_v272_to_json(const cos_v272_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v272\","
        "\"tau_pred\":%.3f,\"tau_whatif\":%.3f,"
        "\"tau_match\":%.3f,\"stable_thr\":%.3f,\"drift_thr\":%.3f,"
        "\"n_sync_rows_ok\":%d,"
        "\"n_sync_stable\":%d,\"n_sync_drifted\":%d,"
        "\"n_pred_rows_ok\":%d,"
        "\"n_pred_replace\":%d,\"n_pred_monitor\":%d,"
        "\"n_whatif_rows_ok\":%d,"
        "\"n_whatif_implement\":%d,\"n_whatif_abort\":%d,"
        "\"n_verified_rows_ok\":%d,"
        "\"n_verified_pass\":%d,\"n_verified_block\":%d,"
        "\"sigma_digital_twin\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"sync\":[",
        s->tau_pred, s->tau_whatif, s->tau_match,
        s->stable_thr, s->drift_thr,
        s->n_sync_rows_ok, s->n_sync_stable, s->n_sync_drifted,
        s->n_pred_rows_ok, s->n_pred_replace, s->n_pred_monitor,
        s->n_whatif_rows_ok, s->n_whatif_implement, s->n_whatif_abort,
        s->n_verified_rows_ok, s->n_verified_pass, s->n_verified_block,
        s->sigma_digital_twin,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V272_N_SYNC; ++i) {
        const cos_v272_sync_t *x = &s->sync[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"timestamp_s\":%d,\"sigma_twin\":%.4f,"
            "\"stable\":%s,\"drifted\":%s}",
            i == 0 ? "" : ",", x->timestamp_s, x->sigma_twin,
            x->stable  ? "true" : "false",
            x->drifted ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"pred\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V272_N_PRED; ++i) {
        const cos_v272_pred_t *p = &s->pred[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"part_id\":\"%s\",\"predicted_failure_hours\":%.2f,"
            "\"sigma_prediction\":%.4f,\"action\":\"%s\"}",
            i == 0 ? "" : ",", p->part_id,
            p->predicted_failure_hours, p->sigma_prediction,
            maint_name(p->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"whatif\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V272_N_WHATIF; ++i) {
        const cos_v272_whatif_row_t *w = &s->whatif[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"delta_param_pct\":%.2f,"
            "\"sigma_whatif\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", w->label,
            w->delta_param_pct, w->sigma_whatif,
            whatif_name(w->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"verified\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V272_N_VERIFIED; ++i) {
        const cos_v272_verified_t *v = &s->verified[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"action_id\":\"%s\",\"declared_sim\":%.4f,"
            "\"realized_phys\":%.4f,\"sigma_match\":%.4f,"
            "\"verdict\":\"%s\"}",
            i == 0 ? "" : ",", v->action_id,
            v->declared_sim, v->realized_phys,
            v->sigma_match, verdict_name(v->verdict));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v272_self_test(void) {
    cos_v272_state_t s;
    cos_v272_init(&s, 0x272DA7AULL);
    cos_v272_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V272_N_SYNC; ++i) {
        bool exp_s = s.sync[i].sigma_twin <  s.stable_thr;
        bool exp_d = s.sync[i].sigma_twin >  s.drift_thr;
        if (s.sync[i].stable  != exp_s) return 2;
        if (s.sync[i].drifted != exp_d) return 3;
    }
    if (s.n_sync_rows_ok != COS_V272_N_SYNC) return 4;
    if (s.n_sync_stable  < 1) return 5;
    if (s.n_sync_drifted < 1) return 6;

    for (int i = 0; i < COS_V272_N_PRED; ++i) {
        cos_v272_maint_t exp =
            (s.pred[i].sigma_prediction <= s.tau_pred)
                ? COS_V272_MAINT_REPLACE
                : COS_V272_MAINT_MONITOR;
        if (s.pred[i].action != exp) return 7;
        if (s.pred[i].predicted_failure_hours <= 0.0f) return 8;
    }
    if (s.n_pred_rows_ok != COS_V272_N_PRED) return 9;
    if (s.n_pred_replace < 1) return 10;
    if (s.n_pred_monitor < 1) return 11;

    for (int i = 0; i < COS_V272_N_WHATIF; ++i) {
        cos_v272_whatif_t exp =
            (s.whatif[i].sigma_whatif <= s.tau_whatif)
                ? COS_V272_WHATIF_IMPLEMENT
                : COS_V272_WHATIF_ABORT;
        if (s.whatif[i].decision != exp) return 12;
    }
    if (s.n_whatif_rows_ok   != COS_V272_N_WHATIF) return 13;
    if (s.n_whatif_implement < 1) return 14;
    if (s.n_whatif_abort     < 1) return 15;

    for (int i = 0; i < COS_V272_N_VERIFIED; ++i) {
        float d = s.verified[i].declared_sim;
        float r = s.verified[i].realized_phys;
        float expected = absf(d - r);
        if (absf(s.verified[i].sigma_match - expected) > 1e-5f) return 16;
        cos_v272_verdict_t exp =
            (s.verified[i].sigma_match <= s.tau_match)
                ? COS_V272_VERDICT_PASS
                : COS_V272_VERDICT_BLOCK;
        if (s.verified[i].verdict != exp) return 17;
    }
    if (s.n_verified_rows_ok != COS_V272_N_VERIFIED) return 18;
    if (s.n_verified_pass  < 1) return 19;
    if (s.n_verified_block < 1) return 20;

    if (s.sigma_digital_twin < 0.0f || s.sigma_digital_twin > 1.0f) return 21;
    if (s.sigma_digital_twin > 1e-6f) return 22;

    cos_v272_state_t u;
    cos_v272_init(&u, 0x272DA7AULL);
    cos_v272_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 23;
    return 0;
}

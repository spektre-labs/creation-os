/*
 * v210 σ-Guardian — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "guardian.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v210_init(cos_v210_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x210CA12DULL;
    s->tau_l2            = 0.25f;
    s->tau_l3            = 0.50f;
    s->tau_l4            = 0.75f;
    s->primary_model_id  = 777;   /* bitnet-b1.58 */
    s->guardian_model_id = 999;   /* distinct model */
}

typedef struct { float baseline, anomaly; int owasp; } fx_t;
/* 20 windows: plenty of benign L1, several L2/L3, ≥ 1 L4.
 * Baseline mean stays below anomaly mean by construction. */
static const fx_t FX[COS_V210_N_WINDOWS] = {
    /* baseline anomaly owasp  (0 = benign) */
    { 0.08f, 0.10f,  0 },       /*  0 L1 */
    { 0.10f, 0.12f,  0 },       /*  1 L1 */
    { 0.12f, 0.15f,  0 },       /*  2 L1 */
    { 0.09f, 0.18f,  0 },       /*  3 L1 */
    { 0.14f, 0.22f,  0 },       /*  4 L1 */
    { 0.12f, 0.30f,  7 },       /*  5 L2 context injection */
    { 0.10f, 0.35f,  4 },       /*  6 L2 memory poisoning */
    { 0.15f, 0.40f,  2 },       /*  7 L2 tool misuse */
    { 0.12f, 0.45f,  5 },       /*  8 L2 cascading */
    { 0.18f, 0.55f,  3 },       /*  9 L3 identity abuse */
    { 0.20f, 0.60f, 10 },       /* 10 L3 supply chain */
    { 0.16f, 0.65f,  1 },       /* 11 L3 goal hijacking */
    { 0.22f, 0.70f,  9 },       /* 12 L3 covert channels */
    { 0.18f, 0.72f,  8 },       /* 13 L3 privilege escalation */
    { 0.30f, 0.95f,  6 },       /* 14 L4 rogue agent */
    { 0.15f, 0.33f,  7 },       /* 15 L2 context injection */
    { 0.12f, 0.20f,  0 },       /* 16 L1 */
    { 0.10f, 0.14f,  0 },       /* 17 L1 */
    { 0.20f, 0.58f,  5 },       /* 18 L3 cascading */
    { 0.35f, 0.98f,  6 }        /* 19 L4 rogue agent */
};

void cos_v210_build(cos_v210_state_t *s) {
    s->n = COS_V210_N_WINDOWS;
    for (int i = 0; i < s->n; ++i) {
        cos_v210_window_t *w = &s->windows[i];
        memset(w, 0, sizeof(*w));
        w->id                = i;
        w->primary_model_id  = s->primary_model_id;
        w->guardian_model_id = s->guardian_model_id;
        w->sigma_baseline    = FX[i].baseline;
        w->sigma_anomaly     = FX[i].anomaly;
        w->owasp_category    = FX[i].owasp;
    }
}

void cos_v210_run(cos_v210_state_t *s) {
    memset(s->per_level_count, 0, sizeof(s->per_level_count));
    memset(s->per_owasp_hits,  0, sizeof(s->per_owasp_hits));

    float bsum = 0.0f, asum = 0.0f;

    uint64_t prev = 0x210C1B4EULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v210_window_t *w = &s->windows[i];

        /* σ_guardian = 0.3·baseline + 0.7·anomaly clamp [0,1]. */
        float g = 0.3f * w->sigma_baseline + 0.7f * w->sigma_anomaly;
        if (g < 0.0f) g = 0.0f;
        if (g > 1.0f) g = 1.0f;
        w->sigma_guardian = g;

        int lvl;
        if      (g > s->tau_l4) lvl = COS_V210_L4_KILL_PROCESS;
        else if (g > s->tau_l3) lvl = COS_V210_L3_BLOCK_ACTION;
        else if (g > s->tau_l2) lvl = COS_V210_L2_WARN_USER;
        else                     lvl = COS_V210_L1_LOG_CONTINUE;
        w->escalation_level = lvl;

        s->per_level_count[lvl]++;
        if (w->owasp_category >= 1 && w->owasp_category <= COS_V210_OWASP_N)
            s->per_owasp_hits[w->owasp_category]++;

        bsum += w->sigma_baseline;
        asum += w->sigma_anomaly;

        w->hash_prev = prev;
        struct { int id, pid, gid, owasp, level;
                 float b, a, g;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = w->id;
        rec.pid   = w->primary_model_id;
        rec.gid   = w->guardian_model_id;
        rec.owasp = w->owasp_category;
        rec.level = w->escalation_level;
        rec.b     = w->sigma_baseline;
        rec.a     = w->sigma_anomaly;
        rec.g     = w->sigma_guardian;
        rec.prev  = prev;
        w->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = w->hash_curr;
    }
    s->terminal_hash = prev;
    s->baseline_mean = bsum / (float)s->n;
    s->anomaly_mean  = asum / (float)s->n;

    uint64_t v = 0x210C1B4EULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v210_window_t *w = &s->windows[i];
        if (w->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, pid, gid, owasp, level;
                 float b, a, g;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = w->id;
        rec.pid   = w->primary_model_id;
        rec.gid   = w->guardian_model_id;
        rec.owasp = w->owasp_category;
        rec.level = w->escalation_level;
        rec.b     = w->sigma_baseline;
        rec.a     = w->sigma_anomaly;
        rec.g     = w->sigma_guardian;
        rec.prev  = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != w->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v210_to_json(const cos_v210_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v210\","
        "\"n\":%d,\"primary_model_id\":%d,\"guardian_model_id\":%d,"
        "\"tau_l2\":%.3f,\"tau_l3\":%.3f,\"tau_l4\":%.3f,"
        "\"per_level_count\":[%d,%d,%d,%d],"
        "\"baseline_mean\":%.3f,\"anomaly_mean\":%.3f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"windows\":[",
        s->n, s->primary_model_id, s->guardian_model_id,
        s->tau_l2, s->tau_l3, s->tau_l4,
        s->per_level_count[1], s->per_level_count[2],
        s->per_level_count[3], s->per_level_count[4],
        s->baseline_mean, s->anomaly_mean,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v210_window_t *w = &s->windows[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"pid\":%d,\"gid\":%d,\"owasp\":%d,"
            "\"level\":%d,\"b\":%.3f,\"a\":%.3f,\"g\":%.3f}",
            i == 0 ? "" : ",", w->id,
            w->primary_model_id, w->guardian_model_id,
            w->owasp_category, w->escalation_level,
            w->sigma_baseline, w->sigma_anomaly, w->sigma_guardian);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v210_self_test(void) {
    cos_v210_state_t s;
    cos_v210_init(&s, 0x210CA12DULL);
    cos_v210_build(&s);
    cos_v210_run(&s);

    if (s.n != COS_V210_N_WINDOWS)         return 1;
    if (s.primary_model_id == s.guardian_model_id) return 2;
    if (!s.chain_valid)                     return 3;
    if (s.per_level_count[COS_V210_L3_BLOCK_ACTION] < 1) return 4;
    if (s.per_level_count[COS_V210_L4_KILL_PROCESS] < 1) return 5;
    if (!(s.anomaly_mean > s.baseline_mean + 1e-6f)) return 6;

    for (int i = 0; i < s.n; ++i) {
        const cos_v210_window_t *w = &s.windows[i];
        if (w->sigma_baseline < 0.0f || w->sigma_baseline > 1.0f) return 7;
        if (w->sigma_anomaly  < 0.0f || w->sigma_anomaly  > 1.0f) return 7;
        if (w->sigma_guardian < 0.0f || w->sigma_guardian > 1.0f) return 7;
        if (w->escalation_level > COS_V210_L1_LOG_CONTINUE &&
            (w->owasp_category < 1 || w->owasp_category > COS_V210_OWASP_N))
            return 8;
    }
    return 0;
}

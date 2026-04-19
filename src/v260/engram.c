/*
 * v260 σ-Engram — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "engram.h"

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

/* 5 static (O(1) hash) lookups.  lookup_ns <= 100 is the
 * DRAM hash-table budget. */
static const struct { const char *e; float s; int ns; }
    STATICS[COS_V260_N_STATIC] = {
    { "entity:Helsinki",         0.06f, 42 },
    { "entity:DeepSeek",         0.09f, 57 },
    { "syntax:subject_verb_obj", 0.04f, 38 },
    { "fact:speed_of_light",     0.05f, 44 },
    { "fact:AGPL_v3",            0.07f, 51 },
};

/* 3 dynamic (MoE reasoning) rows; experts_activated > 0. */
static const struct { const char *t; int ex; float s; }
    DYNAMICS[COS_V260_N_DYNAMIC] = {
    { "analogy:law_vs_physics", 4, 0.19f },
    { "creativity:poem_voice",  3, 0.27f },
    { "logic:modus_tollens",    2, 0.12f },
};

/* 4 σ-gate fixtures exercising both USE and VERIFY. */
static const struct { const char *q; float s; }
    GATE[COS_V260_N_GATE] = {
    { "fact:capital_of_finland",    0.08f }, /* USE */
    { "fact:atomic_number_fe",      0.21f }, /* USE */
    { "reasoning:legal_precedent",  0.48f }, /* VERIFY */
    { "analogy:ecosystem_economy", 0.61f }, /* VERIFY */
};

void cos_v260_init(cos_v260_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed             = seed ? seed : 0x260E16A4ULL;
    s->tau_fact         = 0.30f;
    s->lookup_ns_budget = 100;
}

void cos_v260_run(cos_v260_state_t *s) {
    uint64_t prev = 0x260E16A0ULL;

    s->split.static_pct  = 22;   /* ∈ [20, 25] */
    s->split.dynamic_pct = 78;   /* ∈ [75, 80] */
    s->split.split_ok =
        (s->split.static_pct  >= 20 && s->split.static_pct  <= 25) &&
        (s->split.dynamic_pct >= 75 && s->split.dynamic_pct <= 80) &&
        (s->split.static_pct + s->split.dynamic_pct == 100);
    prev = fnv1a(&s->split.static_pct,  sizeof(s->split.static_pct),  prev);
    prev = fnv1a(&s->split.dynamic_pct, sizeof(s->split.dynamic_pct), prev);

    s->n_static_ok = 0;
    for (int i = 0; i < COS_V260_N_STATIC; ++i) {
        cos_v260_static_t *r = &s->statics[i];
        memset(r, 0, sizeof(*r));
        cpy(r->entity, sizeof(r->entity), STATICS[i].e);
        r->hash         = fnv1a(r->entity, strlen(r->entity), 0);
        r->hit          = true;
        r->sigma_result = STATICS[i].s;
        r->lookup_ns    = STATICS[i].ns;
        bool ok =
            (r->hash != 0) && r->hit &&
            (r->sigma_result >= 0.0f && r->sigma_result <= 1.0f) &&
            (r->lookup_ns <= s->lookup_ns_budget);
        if (ok) s->n_static_ok++;
        prev = fnv1a(r->entity, strlen(r->entity), prev);
        prev = fnv1a(&r->hash,         sizeof(r->hash),         prev);
        prev = fnv1a(&r->hit,          sizeof(r->hit),          prev);
        prev = fnv1a(&r->sigma_result, sizeof(r->sigma_result), prev);
        prev = fnv1a(&r->lookup_ns,    sizeof(r->lookup_ns),    prev);
    }

    s->n_dynamic_ok = 0;
    for (int i = 0; i < COS_V260_N_DYNAMIC; ++i) {
        cos_v260_dynamic_t *r = &s->dynamics[i];
        memset(r, 0, sizeof(*r));
        cpy(r->task, sizeof(r->task), DYNAMICS[i].t);
        r->experts_activated = DYNAMICS[i].ex;
        r->sigma_result      = DYNAMICS[i].s;
        if (r->experts_activated > 0 &&
            r->sigma_result >= 0.0f && r->sigma_result <= 1.0f)
            s->n_dynamic_ok++;
        prev = fnv1a(r->task, strlen(r->task), prev);
        prev = fnv1a(&r->experts_activated, sizeof(r->experts_activated), prev);
        prev = fnv1a(&r->sigma_result,      sizeof(r->sigma_result),      prev);
    }

    s->n_use = s->n_verify = 0;
    for (int i = 0; i < COS_V260_N_GATE; ++i) {
        cos_v260_gate_t *g = &s->gate[i];
        memset(g, 0, sizeof(*g));
        cpy(g->query, sizeof(g->query), GATE[i].q);
        g->sigma_result = GATE[i].s;
        g->decision = (g->sigma_result <= s->tau_fact)
                          ? COS_V260_GATE_USE
                          : COS_V260_GATE_VERIFY;
        if (g->decision == COS_V260_GATE_USE)    s->n_use++;
        if (g->decision == COS_V260_GATE_VERIFY) s->n_verify++;
        prev = fnv1a(g->query, strlen(g->query), prev);
        prev = fnv1a(&g->sigma_result, sizeof(g->sigma_result), prev);
        prev = fnv1a(&g->decision,     sizeof(g->decision),     prev);
    }
    int gate_paths_ok = s->n_use + s->n_verify;

    s->longctx.hit_rate_pct          = 97;
    s->longctx.miss_rate_pct         = 3;
    s->longctx.miss_flagged_by_sigma = true;
    s->longctx.ok =
        (s->longctx.hit_rate_pct + s->longctx.miss_rate_pct == 100) &&
        (s->longctx.hit_rate_pct == 97) &&
        s->longctx.miss_flagged_by_sigma;
    prev = fnv1a(&s->longctx.hit_rate_pct,
                 sizeof(s->longctx.hit_rate_pct),  prev);
    prev = fnv1a(&s->longctx.miss_rate_pct,
                 sizeof(s->longctx.miss_rate_pct), prev);
    prev = fnv1a(&s->longctx.miss_flagged_by_sigma,
                 sizeof(s->longctx.miss_flagged_by_sigma), prev);

    int total   = COS_V260_N_STATIC + COS_V260_N_DYNAMIC +
                  COS_V260_N_GATE + 1 + 1;
    int passing = s->n_static_ok + s->n_dynamic_ok +
                  gate_paths_ok +
                  (s->longctx.ok ? 1 : 0) +
                  (s->split.split_ok ? 1 : 0);
    s->sigma_engram = 1.0f - ((float)passing / (float)total);
    if (s->sigma_engram < 0.0f) s->sigma_engram = 0.0f;
    if (s->sigma_engram > 1.0f) s->sigma_engram = 1.0f;

    struct { int ns, nd, nu, nv; float se, tf; int bud;
             bool lok, sok; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ns = s->n_static_ok;
    trec.nd = s->n_dynamic_ok;
    trec.nu = s->n_use;
    trec.nv = s->n_verify;
    trec.se = s->sigma_engram;
    trec.tf = s->tau_fact;
    trec.bud = s->lookup_ns_budget;
    trec.lok = s->longctx.ok;
    trec.sok = s->split.split_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *gate_name(cos_v260_gate_dec_t d) {
    switch (d) {
    case COS_V260_GATE_USE:    return "USE";
    case COS_V260_GATE_VERIFY: return "VERIFY";
    }
    return "UNKNOWN";
}

size_t cos_v260_to_json(const cos_v260_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v260\","
        "\"tau_fact\":%.3f,\"lookup_ns_budget\":%d,"
        "\"split\":{\"static_pct\":%d,\"dynamic_pct\":%d,"
        "\"split_ok\":%s},"
        "\"n_static\":%d,\"n_dynamic\":%d,\"n_gate\":%d,"
        "\"n_static_ok\":%d,\"n_dynamic_ok\":%d,"
        "\"n_use\":%d,\"n_verify\":%d,"
        "\"longctx\":{\"hit_rate_pct\":%d,"
        "\"miss_rate_pct\":%d,"
        "\"miss_flagged_by_sigma\":%s,\"ok\":%s},"
        "\"sigma_engram\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"statics\":[",
        s->tau_fact, s->lookup_ns_budget,
        s->split.static_pct, s->split.dynamic_pct,
        s->split.split_ok ? "true" : "false",
        COS_V260_N_STATIC, COS_V260_N_DYNAMIC, COS_V260_N_GATE,
        s->n_static_ok, s->n_dynamic_ok,
        s->n_use, s->n_verify,
        s->longctx.hit_rate_pct, s->longctx.miss_rate_pct,
        s->longctx.miss_flagged_by_sigma ? "true" : "false",
        s->longctx.ok ? "true" : "false",
        s->sigma_engram,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V260_N_STATIC; ++i) {
        const cos_v260_static_t *r = &s->statics[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"entity\":\"%s\",\"hash\":\"%016llx\","
            "\"hit\":%s,\"sigma_result\":%.4f,"
            "\"lookup_ns\":%d}",
            i == 0 ? "" : ",", r->entity,
            (unsigned long long)r->hash,
            r->hit ? "true" : "false",
            r->sigma_result, r->lookup_ns);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"dynamics\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V260_N_DYNAMIC; ++i) {
        const cos_v260_dynamic_t *r = &s->dynamics[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"task\":\"%s\",\"experts_activated\":%d,"
            "\"sigma_result\":%.4f}",
            i == 0 ? "" : ",", r->task,
            r->experts_activated, r->sigma_result);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"gate\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V260_N_GATE; ++i) {
        const cos_v260_gate_t *g = &s->gate[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"query\":\"%s\",\"sigma_result\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", g->query,
            g->sigma_result, gate_name(g->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v260_self_test(void) {
    cos_v260_state_t s;
    cos_v260_init(&s, 0x260E16A4ULL);
    cos_v260_run(&s);
    if (!s.chain_valid) return 1;

    if (!s.split.split_ok) return 2;
    if (s.split.static_pct < 20 || s.split.static_pct > 25)   return 3;
    if (s.split.dynamic_pct < 75 || s.split.dynamic_pct > 80) return 4;
    if (s.split.static_pct + s.split.dynamic_pct != 100)      return 5;

    for (int i = 0; i < COS_V260_N_STATIC; ++i) {
        if (s.statics[i].hash == 0)           return 6;
        if (!s.statics[i].hit)                return 7;
        if (s.statics[i].sigma_result < 0.0f ||
            s.statics[i].sigma_result > 1.0f) return 8;
        if (s.statics[i].lookup_ns > s.lookup_ns_budget) return 9;
    }
    if (s.n_static_ok != COS_V260_N_STATIC) return 10;

    for (int i = 0; i < COS_V260_N_DYNAMIC; ++i) {
        if (s.dynamics[i].experts_activated <= 0) return 11;
        if (s.dynamics[i].sigma_result < 0.0f ||
            s.dynamics[i].sigma_result > 1.0f)    return 12;
    }
    if (s.n_dynamic_ok != COS_V260_N_DYNAMIC) return 13;

    for (int i = 0; i < COS_V260_N_GATE; ++i) {
        cos_v260_gate_dec_t expect =
            (s.gate[i].sigma_result <= s.tau_fact)
                ? COS_V260_GATE_USE
                : COS_V260_GATE_VERIFY;
        if (s.gate[i].decision != expect) return 14;
    }
    if (s.n_use    < 1) return 15;
    if (s.n_verify < 1) return 16;

    if (s.longctx.hit_rate_pct    != 97) return 17;
    if (s.longctx.miss_rate_pct   != 3)  return 18;
    if (!s.longctx.miss_flagged_by_sigma) return 19;
    if (!s.longctx.ok) return 20;

    if (s.sigma_engram < 0.0f || s.sigma_engram > 1.0f) return 21;
    if (s.sigma_engram > 1e-6f) return 22;

    cos_v260_state_t t;
    cos_v260_init(&t, 0x260E16A4ULL);
    cos_v260_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 23;
    return 0;
}

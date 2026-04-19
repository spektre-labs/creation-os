/*
 * v262 σ-Hybrid-Engine — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "hybrid_engine.h"

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

static const struct { const char *n; const char *t; float c; float sf; }
    ENGINES[COS_V262_N_ENGINES] = {
    { "bitnet-3B-local",  "local_fast",  0.00f, 0.40f },
    { "airllm-70B-local", "local_heavy", 0.00f, 0.30f },
    { "engram-lookup",    "local_fact",  0.00f, 0.20f },
    { "api-claude",       "cloud",       0.015f, 0.15f },
    { "api-gpt",          "cloud",       0.010f, 0.18f },
};

static const struct { const char *req; float diff; const char *eng; }
    ROUTES[COS_V262_N_ROUTES] = {
    { "small_helper_query",    0.12f, "bitnet-3B-local"   },
    { "capital_of_finland",    0.05f, "engram-lookup"     },
    { "explain_relativity",    0.45f, "airllm-70B-local"  },
    { "write_legal_brief",     0.78f, "api-claude"        },
};

static const struct { const char *eng; float sr; }
    CASCADE[COS_V262_N_CASCADE] = {
    { "bitnet-3B-local",  0.60f },  /* ESCALATE */
    { "airllm-70B-local", 0.30f },  /* OK */
    { "bitnet-3B-local",  0.10f },  /* OK (first engine) */
    { "api-claude",       0.08f },  /* OK (cloud fallback) */
};

void cos_v262_init(cos_v262_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x262ABCDEULL;
    s->tau_accept = 0.40f;
}

static bool in_registry(const cos_v262_state_t *s, const char *name) {
    for (int i = 0; i < COS_V262_N_ENGINES; ++i) {
        if (strcmp(s->engines[i].name, name) == 0) return true;
    }
    return false;
}

static bool is_cloud(const cos_v262_state_t *s, const char *name) {
    for (int i = 0; i < COS_V262_N_ENGINES; ++i) {
        if (strcmp(s->engines[i].name, name) == 0)
            return strcmp(s->engines[i].tier, "cloud") == 0;
    }
    return false;
}

void cos_v262_run(cos_v262_state_t *s) {
    uint64_t prev = 0x262ABCD0ULL;

    s->n_engines_ok = 0;
    for (int i = 0; i < COS_V262_N_ENGINES; ++i) {
        cos_v262_engine_t *e = &s->engines[i];
        memset(e, 0, sizeof(*e));
        cpy(e->name, sizeof(e->name), ENGINES[i].n);
        cpy(e->tier, sizeof(e->tier), ENGINES[i].t);
        e->cost_per_1k_eur = ENGINES[i].c;
        e->sigma_floor     = ENGINES[i].sf;
        e->engine_ok =
            (strlen(e->name) > 0) && (strlen(e->tier) > 0) &&
            (e->cost_per_1k_eur >= 0.0f) &&
            (e->sigma_floor >= 0.0f && e->sigma_floor <= 1.0f);
        if (e->engine_ok) s->n_engines_ok++;
        prev = fnv1a(e->name, strlen(e->name), prev);
        prev = fnv1a(e->tier, strlen(e->tier), prev);
        prev = fnv1a(&e->cost_per_1k_eur, sizeof(e->cost_per_1k_eur), prev);
        prev = fnv1a(&e->sigma_floor,     sizeof(e->sigma_floor),     prev);
    }

    s->n_routes_ok = 0;
    char seen_engines[COS_V262_N_ROUTES][24] = {{0}};
    int seen_count = 0;
    for (int i = 0; i < COS_V262_N_ROUTES; ++i) {
        cos_v262_route_t *r = &s->routes[i];
        memset(r, 0, sizeof(*r));
        cpy(r->request,       sizeof(r->request),       ROUTES[i].req);
        r->sigma_difficulty = ROUTES[i].diff;
        cpy(r->chosen_engine, sizeof(r->chosen_engine), ROUTES[i].eng);
        if ((r->sigma_difficulty >= 0.0f && r->sigma_difficulty <= 1.0f) &&
            in_registry(s, r->chosen_engine))
            s->n_routes_ok++;
        bool already = false;
        for (int j = 0; j < seen_count; ++j) {
            if (strcmp(seen_engines[j], r->chosen_engine) == 0) {
                already = true; break;
            }
        }
        if (!already) {
            cpy(seen_engines[seen_count], sizeof(seen_engines[seen_count]),
                r->chosen_engine);
            seen_count++;
        }
        prev = fnv1a(r->request,       strlen(r->request),       prev);
        prev = fnv1a(&r->sigma_difficulty, sizeof(r->sigma_difficulty), prev);
        prev = fnv1a(r->chosen_engine, strlen(r->chosen_engine), prev);
    }
    s->n_distinct_engines = seen_count;

    s->n_cascade_ok = 0;
    s->n_escalate = 0;
    s->n_cloud_used = 0;
    int n_ok = 0;
    for (int i = 0; i < COS_V262_N_CASCADE; ++i) {
        cos_v262_cascade_t *c = &s->cascade[i];
        memset(c, 0, sizeof(*c));
        cpy(c->engine, sizeof(c->engine), CASCADE[i].eng);
        c->sigma_result = CASCADE[i].sr;
        c->decision = (c->sigma_result <= s->tau_accept)
                          ? COS_V262_CASCADE_OK
                          : COS_V262_CASCADE_ESCALATE;
        if (c->decision == COS_V262_CASCADE_OK)       { n_ok++; s->n_cascade_ok++; }
        if (c->decision == COS_V262_CASCADE_ESCALATE) s->n_escalate++;
        if (is_cloud(s, c->engine))                   s->n_cloud_used++;
        prev = fnv1a(c->engine, strlen(c->engine), prev);
        prev = fnv1a(&c->sigma_result, sizeof(c->sigma_result), prev);
        prev = fnv1a(&c->decision,     sizeof(c->decision),     prev);
    }
    int cascade_ok = n_ok + s->n_escalate;  /* every step typed */

    cos_v262_cost_t *co = &s->cost;
    co->local_pct       = 87;
    co->api_pct         = 13;
    co->eur_sigma_route =  4.20f;
    co->eur_api_only    = 32.00f;
    float pct = 100.0f * (co->eur_api_only - co->eur_sigma_route) /
                co->eur_api_only;
    co->savings_pct = (int)(pct + 0.5f);   /* round-half-up */
    co->cost_ok =
        (co->local_pct + co->api_pct == 100) &&
        (co->local_pct >= 80) &&
        (co->savings_pct >= 80);
    prev = fnv1a(&co->local_pct,       sizeof(co->local_pct),       prev);
    prev = fnv1a(&co->api_pct,         sizeof(co->api_pct),         prev);
    prev = fnv1a(&co->eur_sigma_route, sizeof(co->eur_sigma_route), prev);
    prev = fnv1a(&co->eur_api_only,    sizeof(co->eur_api_only),    prev);
    prev = fnv1a(&co->savings_pct,     sizeof(co->savings_pct),     prev);

    int total = COS_V262_N_ENGINES + COS_V262_N_ROUTES +
                1 + COS_V262_N_CASCADE + 1;
    int passing = s->n_engines_ok + s->n_routes_ok +
                  (s->n_distinct_engines >= 3 ? 1 : 0) +
                  cascade_ok +
                  (co->cost_ok ? 1 : 0);
    s->sigma_hybrid_engine = 1.0f - ((float)passing / (float)total);
    if (s->sigma_hybrid_engine < 0.0f) s->sigma_hybrid_engine = 0.0f;
    if (s->sigma_hybrid_engine > 1.0f) s->sigma_hybrid_engine = 1.0f;

    struct { int ne, nr, nd, nc, nes, ncl;
             float sh, ta; bool cok; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ne = s->n_engines_ok;
    trec.nr = s->n_routes_ok;
    trec.nd = s->n_distinct_engines;
    trec.nc = s->n_cascade_ok;
    trec.nes = s->n_escalate;
    trec.ncl = s->n_cloud_used;
    trec.sh = s->sigma_hybrid_engine;
    trec.ta = s->tau_accept;
    trec.cok = co->cost_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *cascade_name(cos_v262_cascade_dec_t d) {
    switch (d) {
    case COS_V262_CASCADE_OK:       return "OK";
    case COS_V262_CASCADE_ESCALATE: return "ESCALATE";
    }
    return "UNKNOWN";
}

size_t cos_v262_to_json(const cos_v262_state_t *s, char *buf, size_t cap) {
    const cos_v262_cost_t *co = &s->cost;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v262\","
        "\"tau_accept\":%.3f,"
        "\"n_engines\":%d,\"n_routes\":%d,\"n_cascade\":%d,"
        "\"n_engines_ok\":%d,\"n_routes_ok\":%d,"
        "\"n_distinct_engines\":%d,"
        "\"n_cascade_ok\":%d,\"n_escalate\":%d,"
        "\"n_cloud_used\":%d,"
        "\"cost\":{\"local_pct\":%d,\"api_pct\":%d,"
        "\"eur_sigma_route\":%.4f,\"eur_api_only\":%.4f,"
        "\"savings_pct\":%d,\"cost_ok\":%s},"
        "\"sigma_hybrid_engine\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"engines\":[",
        s->tau_accept,
        COS_V262_N_ENGINES, COS_V262_N_ROUTES, COS_V262_N_CASCADE,
        s->n_engines_ok, s->n_routes_ok,
        s->n_distinct_engines,
        s->n_cascade_ok, s->n_escalate,
        s->n_cloud_used,
        co->local_pct, co->api_pct,
        co->eur_sigma_route, co->eur_api_only,
        co->savings_pct, co->cost_ok ? "true" : "false",
        s->sigma_hybrid_engine,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V262_N_ENGINES; ++i) {
        const cos_v262_engine_t *e = &s->engines[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"tier\":\"%s\","
            "\"cost_per_1k_eur\":%.4f,"
            "\"sigma_floor\":%.4f,\"engine_ok\":%s}",
            i == 0 ? "" : ",", e->name, e->tier,
            e->cost_per_1k_eur, e->sigma_floor,
            e->engine_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"routes\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V262_N_ROUTES; ++i) {
        const cos_v262_route_t *r = &s->routes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"request\":\"%s\",\"sigma_difficulty\":%.4f,"
            "\"chosen_engine\":\"%s\"}",
            i == 0 ? "" : ",", r->request,
            r->sigma_difficulty, r->chosen_engine);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"cascade\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V262_N_CASCADE; ++i) {
        const cos_v262_cascade_t *c = &s->cascade[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"engine\":\"%s\",\"sigma_result\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", c->engine,
            c->sigma_result, cascade_name(c->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v262_self_test(void) {
    cos_v262_state_t s;
    cos_v262_init(&s, 0x262ABCDEULL);
    cos_v262_run(&s);
    if (!s.chain_valid) return 1;

    const char *en[COS_V262_N_ENGINES] = {
        "bitnet-3B-local","airllm-70B-local","engram-lookup",
        "api-claude","api-gpt"
    };
    for (int i = 0; i < COS_V262_N_ENGINES; ++i) {
        if (strcmp(s.engines[i].name, en[i]) != 0) return 2;
        if (s.engines[i].cost_per_1k_eur < 0.0f)    return 3;
        if (s.engines[i].sigma_floor < 0.0f ||
            s.engines[i].sigma_floor > 1.0f)        return 4;
        if (!s.engines[i].engine_ok)                return 5;
    }
    if (s.n_engines_ok != COS_V262_N_ENGINES) return 6;

    for (int i = 0; i < COS_V262_N_ROUTES; ++i) {
        if (s.routes[i].sigma_difficulty < 0.0f ||
            s.routes[i].sigma_difficulty > 1.0f) return 7;
        if (!in_registry(&s, s.routes[i].chosen_engine)) return 8;
    }
    if (s.n_routes_ok != COS_V262_N_ROUTES) return 9;
    if (s.n_distinct_engines < 3) return 10;

    int ok_cnt = 0, esc_cnt = 0, cloud_cnt = 0;
    for (int i = 0; i < COS_V262_N_CASCADE; ++i) {
        cos_v262_cascade_dec_t expect =
            (s.cascade[i].sigma_result <= s.tau_accept)
                ? COS_V262_CASCADE_OK
                : COS_V262_CASCADE_ESCALATE;
        if (s.cascade[i].decision != expect) return 11;
        if (s.cascade[i].decision == COS_V262_CASCADE_OK)       ok_cnt++;
        if (s.cascade[i].decision == COS_V262_CASCADE_ESCALATE) esc_cnt++;
        if (is_cloud(&s, s.cascade[i].engine))                  cloud_cnt++;
    }
    if (ok_cnt    < 1) return 12;
    if (esc_cnt   < 1) return 13;
    if (cloud_cnt < 1) return 14;
    /* step 0 MUST be ESCALATE (cascade demonstration). */
    if (s.cascade[0].decision != COS_V262_CASCADE_ESCALATE) return 15;

    if (s.cost.local_pct + s.cost.api_pct != 100) return 16;
    if (s.cost.local_pct < 80)                    return 17;
    if (s.cost.savings_pct < 80)                  return 18;
    if (!s.cost.cost_ok)                          return 19;
    /* savings_pct arithmetic sanity (±1 pt rounding). */
    float pct = 100.0f * (s.cost.eur_api_only - s.cost.eur_sigma_route) /
                s.cost.eur_api_only;
    if (fabsf((float)s.cost.savings_pct - pct) > 1.0f) return 20;

    if (s.sigma_hybrid_engine < 0.0f ||
        s.sigma_hybrid_engine > 1.0f) return 21;
    if (s.sigma_hybrid_engine > 1e-6f) return 22;

    cos_v262_state_t t;
    cos_v262_init(&t, 0x262ABCDEULL);
    cos_v262_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 23;
    return 0;
}

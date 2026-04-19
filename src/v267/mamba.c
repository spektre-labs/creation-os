/*
 * v267 σ-Mamba — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "mamba.h"

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

static const struct { const char *n; const char *c; int e; float t; }
    BACKENDS[COS_V267_N_BACKENDS] = {
    { "mamba",       "O(n)",   1, 5.0f },
    { "rwkv",        "O(n)",   1, 4.6f },
    { "transformer", "O(n^2)", 2, 1.0f },
};

static const struct { const char *q; float s; }
    ROUTES[COS_V267_N_ROUTES] = {
    { "ctx_summarize",  0.18f },   /* mamba  */
    { "ctx_retrieve",   0.26f },   /* mamba  */
    { "ctx_multistep",  0.55f },   /* transformer */
    { "ctx_reasoning",  0.71f },   /* transformer */
};

static const char *LAYER_ARCH[COS_V267_N_LAYERS] = {
    "mamba", "transformer", "mamba", "transformer",
    "mamba", "transformer", "mamba", "transformer",
};
static const float LAYER_SIGMA[COS_V267_N_LAYERS] = {
    0.11f, 0.22f, 0.14f, 0.19f,
    0.16f, 0.24f, 0.13f, 0.21f,
};

static const struct { const char *t; const char *c;
                      const char *r; float sc, sr; }
    TASKS[COS_V267_N_TASKS] = {
    { "longctx_summarize", "mamba",       "transformer", 0.14f, 0.29f },
    { "incontext_reason",  "transformer", "mamba",       0.18f, 0.47f },
    { "sequence_tag",      "rwkv",        "transformer", 0.16f, 0.24f },
};

void cos_v267_init(cos_v267_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed      = seed ? seed : 0x267BABAULL;
    s->tau_mamba = 0.40f;
}

void cos_v267_run(cos_v267_state_t *s) {
    uint64_t prev = 0x267BAB00ULL;

    s->n_backends_ok = 0;
    float t_transformer = 0.0f;
    for (int i = 0; i < COS_V267_N_BACKENDS; ++i) {
        cos_v267_backend_t *b = &s->backends[i];
        memset(b, 0, sizeof(*b));
        cpy(b->name,       sizeof(b->name),       BACKENDS[i].n);
        cpy(b->complexity, sizeof(b->complexity), BACKENDS[i].c);
        b->exponent       = BACKENDS[i].e;
        b->throughput_rel = BACKENDS[i].t;
        if (strcmp(b->name, "transformer") == 0)
            t_transformer = b->throughput_rel;
        prev = fnv1a(b->name,       strlen(b->name),       prev);
        prev = fnv1a(b->complexity, strlen(b->complexity), prev);
        prev = fnv1a(&b->exponent,       sizeof(b->exponent),       prev);
        prev = fnv1a(&b->throughput_rel, sizeof(b->throughput_rel), prev);
    }
    for (int i = 0; i < COS_V267_N_BACKENDS; ++i) {
        const cos_v267_backend_t *b = &s->backends[i];
        bool ok = true;
        if (strcmp(b->name, "mamba") == 0 || strcmp(b->name, "rwkv") == 0) {
            if (b->exponent != 1) ok = false;
            if (b->throughput_rel <= t_transformer) ok = false;
        } else if (strcmp(b->name, "transformer") == 0) {
            if (b->exponent != 2) ok = false;
        }
        if (b->throughput_rel <= 0.0f) ok = false;
        if (ok) s->n_backends_ok++;
    }

    s->n_routes_ok = 0;
    s->n_mamba_routes = 0;
    s->n_trans_routes = 0;
    for (int i = 0; i < COS_V267_N_ROUTES; ++i) {
        cos_v267_route_t *r = &s->routes[i];
        memset(r, 0, sizeof(*r));
        cpy(r->query, sizeof(r->query), ROUTES[i].q);
        r->sigma_mamba = ROUTES[i].s;
        cpy(r->backend, sizeof(r->backend),
            (r->sigma_mamba <= s->tau_mamba) ? "mamba" : "transformer");
        bool expect_mamba = r->sigma_mamba <= s->tau_mamba;
        bool ok =
            r->sigma_mamba >= 0.0f && r->sigma_mamba <= 1.0f &&
            ((expect_mamba  && strcmp(r->backend, "mamba") == 0) ||
             (!expect_mamba && strcmp(r->backend, "transformer") == 0));
        if (ok) s->n_routes_ok++;
        if (strcmp(r->backend, "mamba") == 0)       s->n_mamba_routes++;
        if (strcmp(r->backend, "transformer") == 0) s->n_trans_routes++;
        prev = fnv1a(r->query, strlen(r->query), prev);
        prev = fnv1a(&r->sigma_mamba, sizeof(r->sigma_mamba), prev);
        prev = fnv1a(r->backend, strlen(r->backend), prev);
    }

    s->n_layers_ok = 0;
    s->n_mamba_layers = s->n_trans_layers = 0;
    for (int i = 0; i < COS_V267_N_LAYERS; ++i) {
        cos_v267_layer_t *l = &s->layers[i];
        memset(l, 0, sizeof(*l));
        l->idx = i;
        cpy(l->arch, sizeof(l->arch), LAYER_ARCH[i]);
        l->sigma_arch = LAYER_SIGMA[i];
        if (l->sigma_arch >= 0.0f && l->sigma_arch <= 1.0f &&
            (strcmp(l->arch, "mamba") == 0 ||
             strcmp(l->arch, "transformer") == 0))
            s->n_layers_ok++;
        if (strcmp(l->arch, "mamba") == 0)       s->n_mamba_layers++;
        if (strcmp(l->arch, "transformer") == 0) s->n_trans_layers++;
        prev = fnv1a(&l->idx,        sizeof(l->idx),        prev);
        prev = fnv1a(l->arch, strlen(l->arch), prev);
        prev = fnv1a(&l->sigma_arch, sizeof(l->sigma_arch), prev);
    }

    s->n_tasks_ok = 0;
    for (int i = 0; i < COS_V267_N_TASKS; ++i) {
        cos_v267_task_t *t = &s->tasks[i];
        memset(t, 0, sizeof(*t));
        cpy(t->task,   sizeof(t->task),   TASKS[i].t);
        cpy(t->chosen, sizeof(t->chosen), TASKS[i].c);
        cpy(t->rival,  sizeof(t->rival),  TASKS[i].r);
        t->sigma_chosen = TASKS[i].sc;
        t->sigma_rival  = TASKS[i].sr;
        if (t->sigma_chosen <= t->sigma_rival &&
            t->sigma_chosen >= 0.0f && t->sigma_chosen <= 1.0f &&
            t->sigma_rival  >= 0.0f && t->sigma_rival  <= 1.0f)
            s->n_tasks_ok++;
        prev = fnv1a(t->task,   strlen(t->task),   prev);
        prev = fnv1a(t->chosen, strlen(t->chosen), prev);
        prev = fnv1a(t->rival,  strlen(t->rival),  prev);
        prev = fnv1a(&t->sigma_chosen, sizeof(t->sigma_chosen), prev);
        prev = fnv1a(&t->sigma_rival,  sizeof(t->sigma_rival),  prev);
    }
    /* Count distinct chosen backends across tasks. */
    s->n_distinct_chosen = 0;
    for (int i = 0; i < COS_V267_N_TASKS; ++i) {
        bool dup = false;
        for (int j = 0; j < i; ++j) {
            if (strcmp(s->tasks[i].chosen, s->tasks[j].chosen) == 0) {
                dup = true; break;
            }
        }
        if (!dup) s->n_distinct_chosen++;
    }

    int total   = COS_V267_N_BACKENDS + COS_V267_N_ROUTES + 1 +
                  COS_V267_N_LAYERS + 1 + COS_V267_N_TASKS + 1;
    int passing = s->n_backends_ok + s->n_routes_ok +
                  ((s->n_mamba_routes >= 1 && s->n_trans_routes >= 1) ? 1 : 0) +
                  s->n_layers_ok +
                  ((s->n_mamba_layers == 4 && s->n_trans_layers == 4) ? 1 : 0) +
                  s->n_tasks_ok +
                  ((s->n_distinct_chosen >= 2) ? 1 : 0);
    s->sigma_mamba_kernel = 1.0f - ((float)passing / (float)total);
    if (s->sigma_mamba_kernel < 0.0f) s->sigma_mamba_kernel = 0.0f;
    if (s->sigma_mamba_kernel > 1.0f) s->sigma_mamba_kernel = 1.0f;

    struct { int nb, nr, nml, ntl, nmr, ntr, ntk, nd;
             float sigma, tau; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nb = s->n_backends_ok;
    trec.nr = s->n_routes_ok;
    trec.nml = s->n_mamba_layers;
    trec.ntl = s->n_trans_layers;
    trec.nmr = s->n_mamba_routes;
    trec.ntr = s->n_trans_routes;
    trec.ntk = s->n_tasks_ok;
    trec.nd  = s->n_distinct_chosen;
    trec.sigma = s->sigma_mamba_kernel;
    trec.tau   = s->tau_mamba;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v267_to_json(const cos_v267_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v267\",\"tau_mamba\":%.3f,"
        "\"n_backends_ok\":%d,\"n_routes_ok\":%d,"
        "\"n_mamba_routes\":%d,\"n_trans_routes\":%d,"
        "\"n_layers_ok\":%d,"
        "\"n_mamba_layers\":%d,\"n_trans_layers\":%d,"
        "\"n_tasks_ok\":%d,\"n_distinct_chosen\":%d,"
        "\"sigma_mamba_kernel\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"backends\":[",
        s->tau_mamba,
        s->n_backends_ok, s->n_routes_ok,
        s->n_mamba_routes, s->n_trans_routes,
        s->n_layers_ok,
        s->n_mamba_layers, s->n_trans_layers,
        s->n_tasks_ok, s->n_distinct_chosen,
        s->sigma_mamba_kernel,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V267_N_BACKENDS; ++i) {
        const cos_v267_backend_t *b = &s->backends[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"complexity\":\"%s\","
            "\"exponent\":%d,\"throughput_rel\":%.3f}",
            i == 0 ? "" : ",", b->name, b->complexity,
            b->exponent, b->throughput_rel);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"routes\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V267_N_ROUTES; ++i) {
        const cos_v267_route_t *r = &s->routes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"query\":\"%s\",\"sigma_mamba\":%.4f,"
            "\"backend\":\"%s\"}",
            i == 0 ? "" : ",", r->query, r->sigma_mamba,
            r->backend);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"layers\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V267_N_LAYERS; ++i) {
        const cos_v267_layer_t *l = &s->layers[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"idx\":%d,\"arch\":\"%s\",\"sigma_arch\":%.4f}",
            i == 0 ? "" : ",", l->idx, l->arch, l->sigma_arch);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"tasks\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V267_N_TASKS; ++i) {
        const cos_v267_task_t *t = &s->tasks[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"task\":\"%s\",\"chosen\":\"%s\","
            "\"rival\":\"%s\",\"sigma_chosen\":%.4f,"
            "\"sigma_rival\":%.4f}",
            i == 0 ? "" : ",", t->task, t->chosen,
            t->rival, t->sigma_chosen, t->sigma_rival);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v267_self_test(void) {
    cos_v267_state_t s;
    cos_v267_init(&s, 0x267BABAULL);
    cos_v267_run(&s);
    if (!s.chain_valid) return 1;

    const char *bn[COS_V267_N_BACKENDS] = { "mamba", "rwkv", "transformer" };
    for (int i = 0; i < COS_V267_N_BACKENDS; ++i) {
        if (strcmp(s.backends[i].name, bn[i]) != 0) return 2;
    }
    if (s.backends[0].exponent != 1) return 3;
    if (s.backends[1].exponent != 1) return 4;
    if (s.backends[2].exponent != 2) return 5;
    if (s.backends[0].throughput_rel <= s.backends[2].throughput_rel) return 6;
    if (s.backends[1].throughput_rel <= s.backends[2].throughput_rel) return 7;
    if (s.n_backends_ok != COS_V267_N_BACKENDS) return 8;

    for (int i = 0; i < COS_V267_N_ROUTES; ++i) {
        const char *exp =
            (s.routes[i].sigma_mamba <= s.tau_mamba) ? "mamba" : "transformer";
        if (strcmp(s.routes[i].backend, exp) != 0) return 9;
    }
    if (s.n_routes_ok != COS_V267_N_ROUTES) return 10;
    if (s.n_mamba_routes < 1) return 11;
    if (s.n_trans_routes < 1) return 12;

    for (int i = 0; i < COS_V267_N_LAYERS; ++i) {
        const char *exp = (i % 2 == 0) ? "mamba" : "transformer";
        if (strcmp(s.layers[i].arch, exp) != 0) return 13;
    }
    if (s.n_layers_ok      != COS_V267_N_LAYERS) return 14;
    if (s.n_mamba_layers   != 4) return 15;
    if (s.n_trans_layers   != 4) return 16;

    for (int i = 0; i < COS_V267_N_TASKS; ++i) {
        if (s.tasks[i].sigma_chosen > s.tasks[i].sigma_rival) return 17;
    }
    if (s.n_tasks_ok != COS_V267_N_TASKS) return 18;
    if (s.n_distinct_chosen < 2) return 19;

    if (s.sigma_mamba_kernel < 0.0f || s.sigma_mamba_kernel > 1.0f) return 20;
    if (s.sigma_mamba_kernel > 1e-6f) return 21;

    cos_v267_state_t u;
    cos_v267_init(&u, 0x267BABAULL);
    cos_v267_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 22;
    return 0;
}

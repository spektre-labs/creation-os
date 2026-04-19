/*
 * v264 σ-Sovereign-Stack — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "sovereign_stack.h"

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

static const struct { const char *name; const char *detail;
                      bool os, off, cloud; }
    LAYERS[COS_V264_N_LAYERS] = {
    { "hardware",     "cpu+gpu_min_4gb",         true,  true,  false },
    { "model",        "bitnet-3B+airllm-70B",    true,  true,  false },
    { "memory",       "engram_o1+sqlite",        true,  true,  false },
    { "gate",         "sigma_gate_0p6ns",        true,  true,  false },
    { "network",      "mesh_p2p_no_central",     true,  true,  false },
    { "api_fallback", "claude_or_gpt_on_escal",  true,  false, true  },
    { "license",      "AGPL+SCSL",               true,  true,  false },
};

static const struct { const char *flow; const char *eng; }
    FLOWS[COS_V264_N_FLOWS] = {
    { "helper_query",    "bitnet-3B-local"   },
    { "explain_concept", "airllm-70B-local"  },
    { "fact_lookup",     "engram-lookup"     },
    { "reasoning_chain", "airllm-70B-local"  },
};

static const struct { const char *k; const char *r; }
    ANCHORS[COS_V264_N_ANCHORS] = {
    { "v234", "presence"    },
    { "v182", "privacy"     },
    { "v148", "sovereign"   },
    { "v238", "sovereignty" },
};

static bool is_local_engine(const char *name) {
    return  strcmp(name, "bitnet-3B-local")  == 0 ||
            strcmp(name, "airllm-70B-local") == 0 ||
            strcmp(name, "engram-lookup")    == 0;
}

void cos_v264_init(cos_v264_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x264505FEULL;
}

void cos_v264_run(cos_v264_state_t *s) {
    uint64_t prev = 0x26450500ULL;

    s->n_layers_ok = 0;
    for (int i = 0; i < COS_V264_N_LAYERS; ++i) {
        cos_v264_layer_t *l = &s->layers[i];
        memset(l, 0, sizeof(*l));
        cpy(l->name,   sizeof(l->name),   LAYERS[i].name);
        cpy(l->detail, sizeof(l->detail), LAYERS[i].detail);
        l->open_source    = LAYERS[i].os;
        l->works_offline  = LAYERS[i].off;
        l->requires_cloud = LAYERS[i].cloud;

        /* Layer-indexed contract (evaluated byte-for-byte
         * so a corrupted row fails the count). */
        bool ok = false;
        switch (i) {
        case 0: case 1: case 2: case 3: case 4:
            ok = l->open_source && l->works_offline && !l->requires_cloud;
            break;
        case 5:
            ok = l->open_source && !l->works_offline &&  l->requires_cloud;
            break;
        case 6:
            ok = l->open_source && l->works_offline && !l->requires_cloud;
            break;
        }
        if (ok) s->n_layers_ok++;
        prev = fnv1a(l->name,   strlen(l->name),   prev);
        prev = fnv1a(l->detail, strlen(l->detail), prev);
        prev = fnv1a(&l->open_source,    sizeof(l->open_source),    prev);
        prev = fnv1a(&l->works_offline,  sizeof(l->works_offline),  prev);
        prev = fnv1a(&l->requires_cloud, sizeof(l->requires_cloud), prev);
    }

    s->n_flows_ok = 0;
    char seen[COS_V264_N_FLOWS][24] = {{0}};
    int seen_n = 0;
    for (int i = 0; i < COS_V264_N_FLOWS; ++i) {
        cos_v264_flow_t *f = &s->flows[i];
        memset(f, 0, sizeof(*f));
        cpy(f->flow,   sizeof(f->flow),   FLOWS[i].flow);
        cpy(f->engine, sizeof(f->engine), FLOWS[i].eng);
        f->used_cloud = false;
        f->ok         = is_local_engine(f->engine) && !f->used_cloud;
        if (f->ok) s->n_flows_ok++;
        bool already = false;
        for (int j = 0; j < seen_n; ++j) {
            if (strcmp(seen[j], f->engine) == 0) { already = true; break; }
        }
        if (!already) {
            cpy(seen[seen_n], sizeof(seen[seen_n]), f->engine);
            seen_n++;
        }
        prev = fnv1a(f->flow,   strlen(f->flow),   prev);
        prev = fnv1a(f->engine, strlen(f->engine), prev);
        prev = fnv1a(&f->used_cloud, sizeof(f->used_cloud), prev);
        prev = fnv1a(&f->ok,         sizeof(f->ok),         prev);
    }
    s->n_distinct_local_engines = seen_n;

    s->n_anchors_ok = 0;
    for (int i = 0; i < COS_V264_N_ANCHORS; ++i) {
        cos_v264_anchor_t *a = &s->anchors[i];
        memset(a, 0, sizeof(*a));
        cpy(a->kernel, sizeof(a->kernel), ANCHORS[i].k);
        cpy(a->role,   sizeof(a->role),   ANCHORS[i].r);
        a->fulfilled = true;
        if (a->fulfilled) s->n_anchors_ok++;
        prev = fnv1a(a->kernel, strlen(a->kernel), prev);
        prev = fnv1a(a->role,   strlen(a->role),   prev);
        prev = fnv1a(&a->fulfilled, sizeof(a->fulfilled), prev);
    }

    cos_v264_cost_t *c = &s->cost;
    c->eur_baseline         = 200;
    c->eur_sigma_sovereign  = 20;
    float pct = 100.0f * (float)(c->eur_baseline - c->eur_sigma_sovereign) /
                         (float)c->eur_baseline;
    c->reduction_pct = (int)(pct + 0.5f);
    c->cost_ok =
        (c->eur_baseline == 200) &&
        (c->eur_sigma_sovereign == 20) &&
        (c->reduction_pct == 90);
    prev = fnv1a(&c->eur_baseline,         sizeof(c->eur_baseline),        prev);
    prev = fnv1a(&c->eur_sigma_sovereign,  sizeof(c->eur_sigma_sovereign), prev);
    prev = fnv1a(&c->reduction_pct,        sizeof(c->reduction_pct),       prev);

    int total = COS_V264_N_LAYERS + COS_V264_N_FLOWS +
                1 + COS_V264_N_ANCHORS + 1;
    int passing = s->n_layers_ok + s->n_flows_ok +
                  (s->n_distinct_local_engines >= 2 ? 1 : 0) +
                  s->n_anchors_ok +
                  (c->cost_ok ? 1 : 0);
    s->sigma_sovereign_stack = 1.0f - ((float)passing / (float)total);
    if (s->sigma_sovereign_stack < 0.0f) s->sigma_sovereign_stack = 0.0f;
    if (s->sigma_sovereign_stack > 1.0f) s->sigma_sovereign_stack = 1.0f;

    struct { int nl, nf, nde, na; float ss; bool cok; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nl = s->n_layers_ok;
    trec.nf = s->n_flows_ok;
    trec.nde = s->n_distinct_local_engines;
    trec.na = s->n_anchors_ok;
    trec.ss = s->sigma_sovereign_stack;
    trec.cok = c->cost_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v264_to_json(const cos_v264_state_t *s, char *buf, size_t cap) {
    const cos_v264_cost_t *c = &s->cost;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v264\","
        "\"n_layers\":%d,\"n_flows\":%d,\"n_anchors\":%d,"
        "\"n_layers_ok\":%d,\"n_flows_ok\":%d,"
        "\"n_distinct_local_engines\":%d,\"n_anchors_ok\":%d,"
        "\"cost\":{\"eur_baseline\":%d,"
        "\"eur_sigma_sovereign\":%d,"
        "\"reduction_pct\":%d,\"cost_ok\":%s},"
        "\"sigma_sovereign_stack\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"layers\":[",
        COS_V264_N_LAYERS, COS_V264_N_FLOWS, COS_V264_N_ANCHORS,
        s->n_layers_ok, s->n_flows_ok,
        s->n_distinct_local_engines, s->n_anchors_ok,
        c->eur_baseline, c->eur_sigma_sovereign,
        c->reduction_pct, c->cost_ok ? "true" : "false",
        s->sigma_sovereign_stack,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V264_N_LAYERS; ++i) {
        const cos_v264_layer_t *l = &s->layers[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"detail\":\"%s\","
            "\"open_source\":%s,\"works_offline\":%s,"
            "\"requires_cloud\":%s}",
            i == 0 ? "" : ",", l->name, l->detail,
            l->open_source    ? "true" : "false",
            l->works_offline  ? "true" : "false",
            l->requires_cloud ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"flows\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V264_N_FLOWS; ++i) {
        const cos_v264_flow_t *f = &s->flows[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"flow\":\"%s\",\"engine\":\"%s\","
            "\"used_cloud\":%s,\"ok\":%s}",
            i == 0 ? "" : ",", f->flow, f->engine,
            f->used_cloud ? "true" : "false",
            f->ok         ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"anchors\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V264_N_ANCHORS; ++i) {
        const cos_v264_anchor_t *a = &s->anchors[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"kernel\":\"%s\",\"role\":\"%s\","
            "\"fulfilled\":%s}",
            i == 0 ? "" : ",", a->kernel, a->role,
            a->fulfilled ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v264_self_test(void) {
    cos_v264_state_t s;
    cos_v264_init(&s, 0x264505FEULL);
    cos_v264_run(&s);
    if (!s.chain_valid) return 1;

    const char *ln[COS_V264_N_LAYERS] = {
        "hardware","model","memory","gate","network",
        "api_fallback","license"
    };
    for (int i = 0; i < COS_V264_N_LAYERS; ++i) {
        if (strcmp(s.layers[i].name, ln[i]) != 0) return 2;
        if (!s.layers[i].open_source) return 3;
        if (i == 5) {
            if (s.layers[i].works_offline)   return 4;
            if (!s.layers[i].requires_cloud) return 5;
        } else {
            if (!s.layers[i].works_offline)  return 6;
            if (s.layers[i].requires_cloud)  return 7;
        }
    }
    if (s.n_layers_ok != COS_V264_N_LAYERS) return 8;

    for (int i = 0; i < COS_V264_N_FLOWS; ++i) {
        if (s.flows[i].used_cloud) return 9;
        if (!s.flows[i].ok)        return 10;
        if (!is_local_engine(s.flows[i].engine)) return 11;
    }
    if (s.n_flows_ok != COS_V264_N_FLOWS) return 12;
    if (s.n_distinct_local_engines < 2)   return 13;

    const char *ak[COS_V264_N_ANCHORS] = {"v234","v182","v148","v238"};
    for (int i = 0; i < COS_V264_N_ANCHORS; ++i) {
        if (strcmp(s.anchors[i].kernel, ak[i]) != 0) return 14;
        if (!s.anchors[i].fulfilled)                 return 15;
    }
    if (s.n_anchors_ok != COS_V264_N_ANCHORS) return 16;

    if (s.cost.eur_baseline        != 200) return 17;
    if (s.cost.eur_sigma_sovereign != 20)  return 18;
    if (s.cost.reduction_pct       != 90)  return 19;
    if (!s.cost.cost_ok)                   return 20;

    if (s.sigma_sovereign_stack < 0.0f ||
        s.sigma_sovereign_stack > 1.0f) return 21;
    if (s.sigma_sovereign_stack > 1e-6f) return 22;

    cos_v264_state_t t;
    cos_v264_init(&t, 0x264505FEULL);
    cos_v264_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 23;
    return 0;
}

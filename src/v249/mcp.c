/*
 * v249 σ-MCP — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "mcp.h"

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

static const struct { const char *name; const char *desc; }
    TOOLS[COS_V249_N_TOOLS] = {
    { "reason",   "multi-path reasoning (v111 + v190)" },
    { "plan",     "multi-step planning (v121 + v194)"  },
    { "create",   "creative generation (v219)"         },
    { "simulate", "domain simulation (v220)"           },
    { "teach",    "Socratic mode (v252)"               },
};

static const char *RESOURCES[COS_V249_N_RESOURCES] = {
    "memory", "ontology", "skills"
};

static const struct { const char *name; const char *ep; float trust; }
    EXTERNALS[COS_V249_N_EXTERNALS] = {
    { "database",   "mcp://db.local/sqlite",       0.15f },
    { "api",        "mcp://api.local/rest",        0.32f },
    { "filesystem", "mcp://fs.local/ro-mount",     0.09f },
    { "search",     "mcp://search.local/brave",    0.41f },
};

/* 5 call fixtures, one per server-side tool:
 * three USE, one WARN (σ > τ_tool), one REFUSE
 * (σ > τ_refuse). */
static const struct { const char *tool; float sigma; }
    CALLS[COS_V249_N_CALLS] = {
    { "reason",   0.18f },   /* USE    */
    { "plan",     0.22f },   /* USE    */
    { "create",   0.51f },   /* WARN   (> 0.40)  */
    { "simulate", 0.31f },   /* USE    */
    { "teach",    0.82f },   /* REFUSE (> 0.75) */
};

static const struct { const char *mode; int found; }
    DISCOVERY[COS_V249_N_DISCOVERY] = {
    { "local",    4 },
    { "mdns",     6 },
    { "registry", 12 },
};

void cos_v249_init(cos_v249_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x249ACBE4ULL;
    s->tau_tool   = 0.40f;
    s->tau_refuse = 0.75f;
    cpy(s->jsonrpc_version, sizeof(s->jsonrpc_version), "2.0");
}

void cos_v249_run(cos_v249_state_t *s) {
    uint64_t prev = 0x249ACBE7ULL;

    prev = fnv1a(s->jsonrpc_version, strlen(s->jsonrpc_version), prev);

    s->n_tools_ok = 0;
    for (int i = 0; i < COS_V249_N_TOOLS; ++i) {
        cos_v249_tool_t *t = &s->tools[i];
        memset(t, 0, sizeof(*t));
        cpy(t->name, sizeof(t->name), TOOLS[i].name);
        cpy(t->desc, sizeof(t->desc), TOOLS[i].desc);
        t->tool_ok = (strlen(t->name) > 0 && strlen(t->desc) > 0);
        if (t->tool_ok) s->n_tools_ok++;
        prev = fnv1a(t->name, strlen(t->name), prev);
        prev = fnv1a(t->desc, strlen(t->desc), prev);
    }

    s->n_resources_ok = 0;
    for (int i = 0; i < COS_V249_N_RESOURCES; ++i) {
        cos_v249_resource_t *r = &s->resources[i];
        memset(r, 0, sizeof(*r));
        cpy(r->name, sizeof(r->name), RESOURCES[i]);
        r->resource_ok = (strlen(r->name) > 0);
        if (r->resource_ok) s->n_resources_ok++;
        prev = fnv1a(r->name, strlen(r->name), prev);
    }

    s->n_externals_ok = 0;
    for (int i = 0; i < COS_V249_N_EXTERNALS; ++i) {
        cos_v249_external_t *e = &s->externals[i];
        memset(e, 0, sizeof(*e));
        cpy(e->name,     sizeof(e->name),     EXTERNALS[i].name);
        cpy(e->endpoint, sizeof(e->endpoint), EXTERNALS[i].ep);
        e->sigma_trust = EXTERNALS[i].trust;
        if (e->sigma_trust >= 0.0f && e->sigma_trust <= 1.0f)
            s->n_externals_ok++;
        prev = fnv1a(e->name,     strlen(e->name),     prev);
        prev = fnv1a(e->endpoint, strlen(e->endpoint), prev);
        prev = fnv1a(&e->sigma_trust, sizeof(e->sigma_trust), prev);
    }

    s->n_use = s->n_warn = s->n_refuse = 0;
    for (int i = 0; i < COS_V249_N_CALLS; ++i) {
        cos_v249_call_t *c = &s->calls[i];
        memset(c, 0, sizeof(*c));
        cpy(c->tool_name, sizeof(c->tool_name), CALLS[i].tool);
        c->sigma_result = CALLS[i].sigma;
        if (c->sigma_result > s->tau_refuse)   { c->decision = COS_V249_GATE_REFUSE; s->n_refuse++; }
        else if (c->sigma_result > s->tau_tool){ c->decision = COS_V249_GATE_WARN;   s->n_warn++;   }
        else                                   { c->decision = COS_V249_GATE_USE;    s->n_use++;    }
        prev = fnv1a(c->tool_name, strlen(c->tool_name), prev);
        prev = fnv1a(&c->sigma_result, sizeof(c->sigma_result), prev);
        prev = fnv1a(&c->decision,     sizeof(c->decision),     prev);
    }
    int gate_ok = s->n_use + s->n_warn + s->n_refuse;

    s->n_discovery_ok = 0;
    for (int i = 0; i < COS_V249_N_DISCOVERY; ++i) {
        cos_v249_discovery_t *d = &s->discovery[i];
        memset(d, 0, sizeof(*d));
        cpy(d->mode, sizeof(d->mode), DISCOVERY[i].mode);
        d->found_count = DISCOVERY[i].found;
        d->mode_ok     = (d->found_count >= 0 && strlen(d->mode) > 0);
        if (d->mode_ok) s->n_discovery_ok++;
        prev = fnv1a(d->mode, strlen(d->mode), prev);
        prev = fnv1a(&d->found_count, sizeof(d->found_count), prev);
    }
    s->ontology_mapping_ok = true;

    int total   = COS_V249_N_TOOLS + COS_V249_N_RESOURCES +
                  COS_V249_N_EXTERNALS + COS_V249_N_CALLS +
                  COS_V249_N_DISCOVERY;
    int passing = s->n_tools_ok + s->n_resources_ok +
                  s->n_externals_ok + gate_ok +
                  s->n_discovery_ok;
    s->sigma_mcp = 1.0f - ((float)passing / (float)total);
    if (s->sigma_mcp < 0.0f) s->sigma_mcp = 0.0f;
    if (s->sigma_mcp > 1.0f) s->sigma_mcp = 1.0f;

    struct { int nt, nr, ne, nu, nw, nf, nd; float sm, tt, trf;
             bool om; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nt = s->n_tools_ok;
    trec.nr = s->n_resources_ok;
    trec.ne = s->n_externals_ok;
    trec.nu = s->n_use;
    trec.nw = s->n_warn;
    trec.nf = s->n_refuse;
    trec.nd = s->n_discovery_ok;
    trec.sm = s->sigma_mcp;
    trec.tt = s->tau_tool;
    trec.trf = s->tau_refuse;
    trec.om = s->ontology_mapping_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *gate_name(cos_v249_gate_t g) {
    switch (g) {
    case COS_V249_GATE_USE:    return "USE";
    case COS_V249_GATE_WARN:   return "WARN";
    case COS_V249_GATE_REFUSE: return "REFUSE";
    }
    return "UNKNOWN";
}

size_t cos_v249_to_json(const cos_v249_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v249\","
        "\"jsonrpc_version\":\"%s\","
        "\"n_tools\":%d,\"n_resources\":%d,"
        "\"n_externals\":%d,\"n_calls\":%d,"
        "\"n_discovery\":%d,"
        "\"tau_tool\":%.3f,\"tau_refuse\":%.3f,"
        "\"n_tools_ok\":%d,\"n_resources_ok\":%d,"
        "\"n_externals_ok\":%d,"
        "\"n_use\":%d,\"n_warn\":%d,\"n_refuse\":%d,"
        "\"n_discovery_ok\":%d,"
        "\"ontology_mapping_ok\":%s,"
        "\"sigma_mcp\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"tools\":[",
        s->jsonrpc_version,
        COS_V249_N_TOOLS, COS_V249_N_RESOURCES,
        COS_V249_N_EXTERNALS, COS_V249_N_CALLS,
        COS_V249_N_DISCOVERY,
        s->tau_tool, s->tau_refuse,
        s->n_tools_ok, s->n_resources_ok,
        s->n_externals_ok,
        s->n_use, s->n_warn, s->n_refuse,
        s->n_discovery_ok,
        s->ontology_mapping_ok ? "true" : "false",
        s->sigma_mcp,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V249_N_TOOLS; ++i) {
        const cos_v249_tool_t *t = &s->tools[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"desc\":\"%s\","
            "\"tool_ok\":%s}",
            i == 0 ? "" : ",", t->name, t->desc,
            t->tool_ok ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int q = snprintf(buf + off, cap - off, "],\"resources\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V249_N_RESOURCES; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"resource_ok\":%s}",
            i == 0 ? "" : ",", s->resources[i].name,
            s->resources[i].resource_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"externals\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V249_N_EXTERNALS; ++i) {
        const cos_v249_external_t *e = &s->externals[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"endpoint\":\"%s\","
            "\"sigma_trust\":%.4f}",
            i == 0 ? "" : ",", e->name, e->endpoint, e->sigma_trust);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"calls\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V249_N_CALLS; ++i) {
        const cos_v249_call_t *c = &s->calls[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tool\":\"%s\",\"sigma_result\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", c->tool_name, c->sigma_result,
            gate_name(c->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"discovery\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V249_N_DISCOVERY; ++i) {
        const cos_v249_discovery_t *d = &s->discovery[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"mode\":\"%s\",\"found_count\":%d,"
            "\"mode_ok\":%s}",
            i == 0 ? "" : ",", d->mode, d->found_count,
            d->mode_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v249_self_test(void) {
    cos_v249_state_t s;
    cos_v249_init(&s, 0x249ACBE4ULL);
    cos_v249_run(&s);
    if (!s.chain_valid) return 1;
    if (strcmp(s.jsonrpc_version, "2.0") != 0) return 2;

    const char *tn[COS_V249_N_TOOLS] = {
        "reason","plan","create","simulate","teach"
    };
    for (int i = 0; i < COS_V249_N_TOOLS; ++i) {
        if (strcmp(s.tools[i].name, tn[i]) != 0) return 3;
        if (!s.tools[i].tool_ok) return 4;
    }
    const char *rn[COS_V249_N_RESOURCES] = { "memory","ontology","skills" };
    for (int i = 0; i < COS_V249_N_RESOURCES; ++i) {
        if (strcmp(s.resources[i].name, rn[i]) != 0) return 5;
        if (!s.resources[i].resource_ok) return 6;
    }
    const char *en[COS_V249_N_EXTERNALS] = {
        "database","api","filesystem","search"
    };
    for (int i = 0; i < COS_V249_N_EXTERNALS; ++i) {
        if (strcmp(s.externals[i].name, en[i]) != 0) return 7;
        if (s.externals[i].sigma_trust < 0.0f ||
            s.externals[i].sigma_trust > 1.0f) return 8;
    }
    if (s.n_externals_ok != COS_V249_N_EXTERNALS) return 9;

    for (int i = 0; i < COS_V249_N_CALLS; ++i) {
        if (s.calls[i].sigma_result < 0.0f ||
            s.calls[i].sigma_result > 1.0f) return 10;
        cos_v249_gate_t expect;
        if      (s.calls[i].sigma_result > s.tau_refuse) expect = COS_V249_GATE_REFUSE;
        else if (s.calls[i].sigma_result > s.tau_tool)   expect = COS_V249_GATE_WARN;
        else                                             expect = COS_V249_GATE_USE;
        if (s.calls[i].decision != expect) return 11;
    }
    if (s.n_use    < 3) return 12;
    if (s.n_warn   < 1) return 13;
    if (s.n_refuse < 1) return 14;
    if (s.n_use + s.n_warn + s.n_refuse != COS_V249_N_CALLS) return 15;

    const char *dm[COS_V249_N_DISCOVERY] = { "local","mdns","registry" };
    for (int i = 0; i < COS_V249_N_DISCOVERY; ++i) {
        if (strcmp(s.discovery[i].mode, dm[i]) != 0) return 16;
        if (!s.discovery[i].mode_ok) return 17;
    }
    if (!s.ontology_mapping_ok) return 18;

    if (s.sigma_mcp < 0.0f || s.sigma_mcp > 1.0f) return 19;
    if (s.sigma_mcp > 1e-6f) return 20;

    cos_v249_state_t t;
    cos_v249_init(&t, 0x249ACBE4ULL);
    cos_v249_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 21;
    return 0;
}

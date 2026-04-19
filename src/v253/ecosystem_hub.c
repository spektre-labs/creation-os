/*
 * v253 σ-Ecosystem-Hub — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ecosystem_hub.h"

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

static const struct { const char *name; const char *up; }
    SECTIONS[COS_V253_N_SECTIONS] = {
    { "marketplace",         "v251" },
    { "agent_directory",     "v250" },
    { "documentation",       "v248" },
    { "community_forum",     "v253" },
    { "benchmark_dashboard", "v247" },
};

static const struct { const char *name; int value; }
    HEALTH[COS_V253_N_HEALTH] = {
    { "active_instances",   128 },
    { "kernels_published",   15 },
    { "a2a_federations",      4 },
    { "contributors_30d",    23 },
};

static const char *CONTRIB_STEPS[COS_V253_N_CONTRIB] = {
    "fork","write_kernel","pull_request","merge_gate","publish"
};

static const struct { const char *title; int votes; bool pc; }
    ROADMAP[COS_V253_N_ROADMAP] = {
    { "embodied_robotics_kernel",  42, false },
    { "vision_kernel_v2",          31, true  },  /* proconductor pick */
    { "audio_kernel_v2",           18, false },
    { "quantum_backend_v1",         9, false },
};

static const char *UNITY_SCOPES[COS_V253_N_UNITY] = {
    "instance","kernel","interaction","a2a_message"
};

void cos_v253_init(cos_v253_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x253EC0AFULL;
    cpy(s->hub_url, sizeof(s->hub_url), "hub.creation-os.dev");
}

void cos_v253_run(cos_v253_state_t *s) {
    uint64_t prev = 0x253EC0A2ULL;

    prev = fnv1a(s->hub_url, strlen(s->hub_url), prev);

    s->n_sections_ok = 0;
    for (int i = 0; i < COS_V253_N_SECTIONS; ++i) {
        cos_v253_section_t *sec = &s->sections[i];
        memset(sec, 0, sizeof(*sec));
        cpy(sec->name,     sizeof(sec->name),     SECTIONS[i].name);
        cpy(sec->upstream, sizeof(sec->upstream), SECTIONS[i].up);
        sec->section_ok = (strlen(sec->name) > 0 &&
                           strlen(sec->upstream) > 0);
        if (sec->section_ok) s->n_sections_ok++;
        prev = fnv1a(sec->name,     strlen(sec->name),     prev);
        prev = fnv1a(sec->upstream, strlen(sec->upstream), prev);
    }

    s->n_health_positive = 0;
    for (int i = 0; i < COS_V253_N_HEALTH; ++i) {
        cos_v253_metric_t *m = &s->health[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), HEALTH[i].name);
        m->value = HEALTH[i].value;
        if (m->value > 0) s->n_health_positive++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(&m->value, sizeof(m->value), prev);
    }

    s->n_contrib_ok = 0;
    for (int i = 0; i < COS_V253_N_CONTRIB; ++i) {
        cos_v253_contrib_t *c = &s->contrib[i];
        memset(c, 0, sizeof(*c));
        cpy(c->step, sizeof(c->step), CONTRIB_STEPS[i]);
        c->step_ok = (strlen(c->step) > 0);
        if (c->step_ok) s->n_contrib_ok++;
        prev = fnv1a(c->step, strlen(c->step), prev);
    }

    s->n_roadmap_voted_in      = 0;
    s->n_proconductor_decisions = 0;
    for (int i = 0; i < COS_V253_N_ROADMAP; ++i) {
        cos_v253_proposal_t *p = &s->roadmap[i];
        memset(p, 0, sizeof(*p));
        cpy(p->title, sizeof(p->title), ROADMAP[i].title);
        p->votes                 = ROADMAP[i].votes;
        p->proconductor_decision = ROADMAP[i].pc;
        if (p->votes > 0)                 s->n_roadmap_voted_in++;
        if (p->proconductor_decision)     s->n_proconductor_decisions++;
        prev = fnv1a(p->title, strlen(p->title), prev);
        prev = fnv1a(&p->votes, sizeof(p->votes), prev);
        prev = fnv1a(&p->proconductor_decision,
                     sizeof(p->proconductor_decision), prev);
    }

    s->n_unity_ok = 0;
    for (int i = 0; i < COS_V253_N_UNITY; ++i) {
        cos_v253_unity_t *u = &s->unity[i];
        memset(u, 0, sizeof(*u));
        cpy(u->scope, sizeof(u->scope), UNITY_SCOPES[i]);
        u->declared = true;
        u->realized = true;
        if (u->declared && u->realized) s->n_unity_ok++;
        prev = fnv1a(u->scope, strlen(u->scope), prev);
    }

    int total   = COS_V253_N_SECTIONS + COS_V253_N_HEALTH +
                  COS_V253_N_CONTRIB  + COS_V253_N_UNITY;
    int passing = s->n_sections_ok + s->n_health_positive +
                  s->n_contrib_ok  + s->n_unity_ok;
    s->sigma_ecosystem = 1.0f - ((float)passing / (float)total);
    if (s->sigma_ecosystem < 0.0f) s->sigma_ecosystem = 0.0f;
    if (s->sigma_ecosystem > 1.0f) s->sigma_ecosystem = 1.0f;

    struct { int ns, nh, nc, nv, np, nu;
             float se; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ns = s->n_sections_ok;
    trec.nh = s->n_health_positive;
    trec.nc = s->n_contrib_ok;
    trec.nv = s->n_roadmap_voted_in;
    trec.np = s->n_proconductor_decisions;
    trec.nu = s->n_unity_ok;
    trec.se = s->sigma_ecosystem;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v253_to_json(const cos_v253_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v253\","
        "\"hub_url\":\"%s\","
        "\"n_sections\":%d,\"n_health\":%d,"
        "\"n_contrib\":%d,\"n_roadmap\":%d,\"n_unity\":%d,"
        "\"n_sections_ok\":%d,\"n_health_positive\":%d,"
        "\"n_contrib_ok\":%d,\"n_roadmap_voted_in\":%d,"
        "\"n_proconductor_decisions\":%d,\"n_unity_ok\":%d,"
        "\"sigma_ecosystem\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"sections\":[",
        s->hub_url,
        COS_V253_N_SECTIONS, COS_V253_N_HEALTH,
        COS_V253_N_CONTRIB, COS_V253_N_ROADMAP, COS_V253_N_UNITY,
        s->n_sections_ok, s->n_health_positive,
        s->n_contrib_ok, s->n_roadmap_voted_in,
        s->n_proconductor_decisions, s->n_unity_ok,
        s->sigma_ecosystem,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V253_N_SECTIONS; ++i) {
        const cos_v253_section_t *x = &s->sections[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"upstream\":\"%s\","
            "\"section_ok\":%s}",
            i == 0 ? "" : ",", x->name, x->upstream,
            x->section_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"health\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V253_N_HEALTH; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"value\":%d}",
            i == 0 ? "" : ",", s->health[i].name, s->health[i].value);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"contrib\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V253_N_CONTRIB; ++i) {
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":\"%s\",\"step_ok\":%s}",
            i == 0 ? "" : ",", s->contrib[i].step,
            s->contrib[i].step_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"roadmap\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V253_N_ROADMAP; ++i) {
        const cos_v253_proposal_t *p = &s->roadmap[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"title\":\"%s\",\"votes\":%d,"
            "\"proconductor_decision\":%s}",
            i == 0 ? "" : ",", p->title, p->votes,
            p->proconductor_decision ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"unity\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V253_N_UNITY; ++i) {
        const cos_v253_unity_t *u = &s->unity[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scope\":\"%s\",\"declared\":%s,\"realized\":%s}",
            i == 0 ? "" : ",", u->scope,
            u->declared ? "true" : "false",
            u->realized ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v253_self_test(void) {
    cos_v253_state_t s;
    cos_v253_init(&s, 0x253EC0AFULL);
    cos_v253_run(&s);
    if (!s.chain_valid) return 1;
    if (strcmp(s.hub_url, "hub.creation-os.dev") != 0) return 2;

    const char *sn[COS_V253_N_SECTIONS] = {
        "marketplace","agent_directory","documentation",
        "community_forum","benchmark_dashboard"
    };
    for (int i = 0; i < COS_V253_N_SECTIONS; ++i) {
        if (strcmp(s.sections[i].name, sn[i]) != 0) return 3;
        if (strlen(s.sections[i].upstream) == 0)    return 4;
        if (!s.sections[i].section_ok)              return 5;
    }
    if (s.n_sections_ok != COS_V253_N_SECTIONS) return 6;

    const char *hn[COS_V253_N_HEALTH] = {
        "active_instances","kernels_published",
        "a2a_federations","contributors_30d"
    };
    for (int i = 0; i < COS_V253_N_HEALTH; ++i) {
        if (strcmp(s.health[i].name, hn[i]) != 0) return 7;
        if (s.health[i].value <= 0) return 8;
    }
    if (s.n_health_positive != COS_V253_N_HEALTH) return 9;

    const char *cn[COS_V253_N_CONTRIB] = {
        "fork","write_kernel","pull_request","merge_gate","publish"
    };
    for (int i = 0; i < COS_V253_N_CONTRIB; ++i) {
        if (strcmp(s.contrib[i].step, cn[i]) != 0) return 10;
        if (!s.contrib[i].step_ok) return 11;
    }
    if (s.n_contrib_ok != COS_V253_N_CONTRIB) return 12;

    if (s.n_roadmap_voted_in < 1)    return 13;
    if (s.n_proconductor_decisions != 1) return 14;

    const char *un[COS_V253_N_UNITY] = {
        "instance","kernel","interaction","a2a_message"
    };
    for (int i = 0; i < COS_V253_N_UNITY; ++i) {
        if (strcmp(s.unity[i].scope, un[i]) != 0) return 15;
        if (!s.unity[i].declared) return 16;
        if (!s.unity[i].realized) return 17;
    }
    if (s.n_unity_ok != COS_V253_N_UNITY) return 18;

    if (s.sigma_ecosystem < 0.0f || s.sigma_ecosystem > 1.0f) return 19;
    if (s.sigma_ecosystem > 1e-6f) return 20;

    cos_v253_state_t t;
    cos_v253_init(&t, 0x253EC0AFULL);
    cos_v253_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 21;
    return 0;
}

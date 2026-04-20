/*
 * v293 σ-Hagia-Sofia — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "hagia_sofia.h"

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

static const char *METRICS[COS_V293_N_METRIC] = {
    "daily_users", "api_calls", "sigma_evaluations",
};

static const char *DOMAINS[COS_V293_N_DOMAIN] = {
    "llm", "sensor", "organization",
};

static const char *COMMUNITIES[COS_V293_N_COMMUNITY] = {
    "open_source_agpl",
    "community_maintainable",
    "vendor_independent",
};

static const struct { const char *p; int c; bool warn; bool newd; }
    LIFECYCLES[COS_V293_N_LIFECYCLE] = {
    { "active_original_purpose", 100, false, false },
    { "declining_usage",          20, true,  false },
    { "repurposed",               80, false, true  },
};

void cos_v293_init(cos_v293_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x293dddULL;
}

void cos_v293_run(cos_v293_state_t *s) {
    uint64_t prev = 0x293ddd00ULL;

    s->n_metric_rows_ok = 0;
    int n_tracked = 0;
    for (int i = 0; i < COS_V293_N_METRIC; ++i) {
        cos_v293_metric_t *m = &s->metric[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), METRICS[i]);
        m->tracked = true;
        if (strlen(m->name) > 0) s->n_metric_rows_ok++;
        if (m->tracked) n_tracked++;
        prev = fnv1a(m->name,    strlen(m->name),    prev);
        prev = fnv1a(&m->tracked, sizeof(m->tracked), prev);
    }
    s->metric_all_tracked_ok = (n_tracked == COS_V293_N_METRIC);

    s->n_domain_rows_ok = 0;
    int n_app = 0;
    for (int i = 0; i < COS_V293_N_DOMAIN; ++i) {
        cos_v293_domain_t *d = &s->domain[i];
        memset(d, 0, sizeof(*d));
        cpy(d->domain, sizeof(d->domain), DOMAINS[i]);
        d->sigma_gate_applicable = true;
        if (strlen(d->domain) > 0) s->n_domain_rows_ok++;
        if (d->sigma_gate_applicable) n_app++;
        prev = fnv1a(d->domain,    strlen(d->domain),    prev);
        prev = fnv1a(&d->sigma_gate_applicable, sizeof(d->sigma_gate_applicable), prev);
    }
    s->domain_all_applicable_ok = (n_app == COS_V293_N_DOMAIN);

    s->n_community_rows_ok = 0;
    int n_hold = 0;
    for (int i = 0; i < COS_V293_N_COMMUNITY; ++i) {
        cos_v293_community_t *c = &s->community[i];
        memset(c, 0, sizeof(*c));
        cpy(c->property, sizeof(c->property), COMMUNITIES[i]);
        c->holds = true;
        if (strlen(c->property) > 0) s->n_community_rows_ok++;
        if (c->holds) n_hold++;
        prev = fnv1a(c->property, strlen(c->property), prev);
        prev = fnv1a(&c->holds,   sizeof(c->holds),   prev);
    }
    s->community_all_hold_ok = (n_hold == COS_V293_N_COMMUNITY);

    s->n_lifecycle_rows_ok = 0;
    int n_alive = 0;
    for (int i = 0; i < COS_V293_N_LIFECYCLE; ++i) {
        cos_v293_lifecycle_t *l = &s->lifecycle[i];
        memset(l, 0, sizeof(*l));
        cpy(l->phase, sizeof(l->phase), LIFECYCLES[i].p);
        l->use_count        = LIFECYCLES[i].c;
        l->alive            = true;
        l->warning_issued   = LIFECYCLES[i].warn;
        l->new_domain_found = LIFECYCLES[i].newd;
        if (strlen(l->phase) > 0 && l->use_count >= 0)
            s->n_lifecycle_rows_ok++;
        if (l->alive) n_alive++;
        prev = fnv1a(l->phase,             strlen(l->phase),             prev);
        prev = fnv1a(&l->use_count,        sizeof(l->use_count),         prev);
        prev = fnv1a(&l->alive,            sizeof(l->alive),             prev);
        prev = fnv1a(&l->warning_issued,   sizeof(l->warning_issued),    prev);
        prev = fnv1a(&l->new_domain_found, sizeof(l->new_domain_found),  prev);
    }
    s->lifecycle_all_alive_ok = (n_alive == COS_V293_N_LIFECYCLE);
    s->lifecycle_warning_ok =
        (strcmp(s->lifecycle[1].phase, "declining_usage") == 0) &&
        (s->lifecycle[1].warning_issued == true);
    s->lifecycle_new_domain_ok =
        (strcmp(s->lifecycle[2].phase, "repurposed") == 0) &&
        (s->lifecycle[2].new_domain_found == true);

    int total   = COS_V293_N_METRIC    + 1 +
                  COS_V293_N_DOMAIN    + 1 +
                  COS_V293_N_COMMUNITY + 1 +
                  COS_V293_N_LIFECYCLE + 1 + 1 + 1;
    int passing = s->n_metric_rows_ok +
                  (s->metric_all_tracked_ok    ? 1 : 0) +
                  s->n_domain_rows_ok +
                  (s->domain_all_applicable_ok ? 1 : 0) +
                  s->n_community_rows_ok +
                  (s->community_all_hold_ok    ? 1 : 0) +
                  s->n_lifecycle_rows_ok +
                  (s->lifecycle_all_alive_ok   ? 1 : 0) +
                  (s->lifecycle_warning_ok     ? 1 : 0) +
                  (s->lifecycle_new_domain_ok  ? 1 : 0);
    s->sigma_hagia = 1.0f - ((float)passing / (float)total);
    if (s->sigma_hagia < 0.0f) s->sigma_hagia = 0.0f;
    if (s->sigma_hagia > 1.0f) s->sigma_hagia = 1.0f;

    struct { int nm, nd, nc, nl;
             bool mt, da, ch, la, lw, ln;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nm = s->n_metric_rows_ok;
    trec.nd = s->n_domain_rows_ok;
    trec.nc = s->n_community_rows_ok;
    trec.nl = s->n_lifecycle_rows_ok;
    trec.mt = s->metric_all_tracked_ok;
    trec.da = s->domain_all_applicable_ok;
    trec.ch = s->community_all_hold_ok;
    trec.la = s->lifecycle_all_alive_ok;
    trec.lw = s->lifecycle_warning_ok;
    trec.ln = s->lifecycle_new_domain_ok;
    trec.sigma = s->sigma_hagia;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v293_to_json(const cos_v293_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v293\","
        "\"n_metric_rows_ok\":%d,\"metric_all_tracked_ok\":%s,"
        "\"n_domain_rows_ok\":%d,\"domain_all_applicable_ok\":%s,"
        "\"n_community_rows_ok\":%d,\"community_all_hold_ok\":%s,"
        "\"n_lifecycle_rows_ok\":%d,"
        "\"lifecycle_all_alive_ok\":%s,"
        "\"lifecycle_warning_ok\":%s,"
        "\"lifecycle_new_domain_ok\":%s,"
        "\"sigma_hagia\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"metric\":[",
        s->n_metric_rows_ok,
        s->metric_all_tracked_ok ? "true" : "false",
        s->n_domain_rows_ok,
        s->domain_all_applicable_ok ? "true" : "false",
        s->n_community_rows_ok,
        s->community_all_hold_ok ? "true" : "false",
        s->n_lifecycle_rows_ok,
        s->lifecycle_all_alive_ok   ? "true" : "false",
        s->lifecycle_warning_ok     ? "true" : "false",
        s->lifecycle_new_domain_ok  ? "true" : "false",
        s->sigma_hagia,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V293_N_METRIC; ++i) {
        const cos_v293_metric_t *m = &s->metric[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"tracked\":%s}",
            i == 0 ? "" : ",", m->name,
            m->tracked ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"domain\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V293_N_DOMAIN; ++i) {
        const cos_v293_domain_t *d = &s->domain[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"domain\":\"%s\",\"sigma_gate_applicable\":%s}",
            i == 0 ? "" : ",", d->domain,
            d->sigma_gate_applicable ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"community\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V293_N_COMMUNITY; ++i) {
        const cos_v293_community_t *c = &s->community[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"property\":\"%s\",\"holds\":%s}",
            i == 0 ? "" : ",", c->property,
            c->holds ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"lifecycle\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V293_N_LIFECYCLE; ++i) {
        const cos_v293_lifecycle_t *l = &s->lifecycle[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"phase\":\"%s\",\"use_count\":%d,"
            "\"alive\":%s,\"warning_issued\":%s,"
            "\"new_domain_found\":%s}",
            i == 0 ? "" : ",", l->phase, l->use_count,
            l->alive            ? "true" : "false",
            l->warning_issued   ? "true" : "false",
            l->new_domain_found ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v293_self_test(void) {
    cos_v293_state_t s;
    cos_v293_init(&s, 0x293dddULL);
    cos_v293_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_M[COS_V293_N_METRIC] = {
        "daily_users", "api_calls", "sigma_evaluations"
    };
    for (int i = 0; i < COS_V293_N_METRIC; ++i) {
        if (strcmp(s.metric[i].name, WANT_M[i]) != 0) return 2;
        if (!s.metric[i].tracked) return 3;
    }
    if (s.n_metric_rows_ok    != COS_V293_N_METRIC) return 4;
    if (!s.metric_all_tracked_ok) return 5;

    static const char *WANT_D[COS_V293_N_DOMAIN] = {
        "llm", "sensor", "organization"
    };
    for (int i = 0; i < COS_V293_N_DOMAIN; ++i) {
        if (strcmp(s.domain[i].domain, WANT_D[i]) != 0) return 6;
        if (!s.domain[i].sigma_gate_applicable) return 7;
    }
    if (s.n_domain_rows_ok       != COS_V293_N_DOMAIN) return 8;
    if (!s.domain_all_applicable_ok) return 9;

    static const char *WANT_C[COS_V293_N_COMMUNITY] = {
        "open_source_agpl", "community_maintainable", "vendor_independent"
    };
    for (int i = 0; i < COS_V293_N_COMMUNITY; ++i) {
        if (strcmp(s.community[i].property, WANT_C[i]) != 0) return 10;
        if (!s.community[i].holds) return 11;
    }
    if (s.n_community_rows_ok    != COS_V293_N_COMMUNITY) return 12;
    if (!s.community_all_hold_ok) return 13;

    static const char *WANT_L[COS_V293_N_LIFECYCLE] = {
        "active_original_purpose", "declining_usage", "repurposed"
    };
    for (int i = 0; i < COS_V293_N_LIFECYCLE; ++i) {
        if (strcmp(s.lifecycle[i].phase, WANT_L[i]) != 0) return 14;
        if (!s.lifecycle[i].alive) return 15;
    }
    if (!s.lifecycle[1].warning_issued)   return 16;
    if (!s.lifecycle[2].new_domain_found) return 17;
    if (s.n_lifecycle_rows_ok        != COS_V293_N_LIFECYCLE) return 18;
    if (!s.lifecycle_all_alive_ok)  return 19;
    if (!s.lifecycle_warning_ok)    return 20;
    if (!s.lifecycle_new_domain_ok) return 21;

    if (s.sigma_hagia < 0.0f || s.sigma_hagia > 1.0f) return 22;
    if (s.sigma_hagia > 1e-6f) return 23;

    cos_v293_state_t u;
    cos_v293_init(&u, 0x293dddULL);
    cos_v293_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}

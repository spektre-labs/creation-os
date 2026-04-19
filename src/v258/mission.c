/*
 * v258 σ-Mission — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "mission.h"

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

static const struct { const char *scope; float sb; float sa; }
    SCOPES[COS_V258_N_SCOPES] = {
    { "per_user",        0.42f, 0.18f },
    { "per_domain",      0.51f, 0.22f },
    { "per_institution", 0.63f, 0.34f },
    { "per_society",     0.71f, 0.48f },
};

/* Anti-drift fixture: two canonical kernels (v29
 * sigma, v254 tutor) are mission-aligned → ACCEPT;
 * two deliberately off-mission stubs → REJECT. */
static const struct { const char *ref; float sm; }
    DRIFT[COS_V258_N_DRIFT] = {
    { "v29_sigma",                      0.08f }, /* ACCEPT */
    { "v254_tutor",                     0.12f }, /* ACCEPT */
    { "v258_stub_marketing_loop",       0.62f }, /* REJECT */
    { "v258_stub_engagement_maximiser", 0.71f }, /* REJECT */
};

static const struct { const char *a; const char *up; }
    ANCHORS[COS_V258_N_ANCHORS] = {
    { "civilization_governance", "v203" },
    { "wisdom_transfer_legacy",  "v233" },
    { "human_sovereignty",       "v238" },
    { "declared_eq_realized",    "1=1"  },
};

void cos_v258_init(cos_v258_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x258FACE0ULL;
    s->tau_mission = 0.30f;
}

void cos_v258_run(cos_v258_state_t *s) {
    uint64_t prev = 0x258FACEFULL;

    cpy(s->mission.statement, sizeof(s->mission.statement),
        COS_V258_MISSION_STATEMENT);
    s->mission.statement_ok =
        (strcmp(s->mission.statement, COS_V258_MISSION_STATEMENT) == 0);
    prev = fnv1a(s->mission.statement,
                 strlen(s->mission.statement), prev);

    s->n_scopes_improved = 0;
    for (int i = 0; i < COS_V258_N_SCOPES; ++i) {
        cos_v258_impact_t *r = &s->scopes[i];
        memset(r, 0, sizeof(*r));
        cpy(r->scope, sizeof(r->scope), SCOPES[i].scope);
        r->sigma_before = SCOPES[i].sb;
        r->sigma_after  = SCOPES[i].sa;
        r->delta_sigma  = r->sigma_before - r->sigma_after;
        r->improved     = (r->delta_sigma > 0.0f);
        if (r->improved) s->n_scopes_improved++;
        prev = fnv1a(r->scope, strlen(r->scope), prev);
        prev = fnv1a(&r->sigma_before, sizeof(r->sigma_before), prev);
        prev = fnv1a(&r->sigma_after,  sizeof(r->sigma_after),  prev);
        prev = fnv1a(&r->delta_sigma,  sizeof(r->delta_sigma),  prev);
    }

    s->n_accept = s->n_reject = 0;
    for (int i = 0; i < COS_V258_N_DRIFT; ++i) {
        cos_v258_drift_t *d = &s->drift[i];
        memset(d, 0, sizeof(*d));
        cpy(d->kernel_ref, sizeof(d->kernel_ref), DRIFT[i].ref);
        d->sigma_mission = DRIFT[i].sm;
        d->decision = (d->sigma_mission <= s->tau_mission)
                          ? COS_V258_DRIFT_ACCEPT
                          : COS_V258_DRIFT_REJECT;
        if (d->decision == COS_V258_DRIFT_ACCEPT) s->n_accept++;
        else                                      s->n_reject++;
        prev = fnv1a(d->kernel_ref, strlen(d->kernel_ref), prev);
        prev = fnv1a(&d->sigma_mission, sizeof(d->sigma_mission), prev);
        prev = fnv1a(&d->decision,      sizeof(d->decision),      prev);
    }
    int drift_gate_ok = s->n_accept + s->n_reject;

    s->n_anchors_ok = 0;
    for (int i = 0; i < COS_V258_N_ANCHORS; ++i) {
        cos_v258_anchor_t *a = &s->anchors[i];
        memset(a, 0, sizeof(*a));
        cpy(a->anchor,   sizeof(a->anchor),   ANCHORS[i].a);
        cpy(a->upstream, sizeof(a->upstream), ANCHORS[i].up);
        a->anchor_ok = (strlen(a->anchor) > 0 && strlen(a->upstream) > 0);
        if (a->anchor_ok) s->n_anchors_ok++;
        prev = fnv1a(a->anchor,   strlen(a->anchor),   prev);
        prev = fnv1a(a->upstream, strlen(a->upstream), prev);
    }

    int total   = 1 + COS_V258_N_SCOPES + COS_V258_N_DRIFT +
                  COS_V258_N_ANCHORS;
    int passing = (s->mission.statement_ok ? 1 : 0) +
                  s->n_scopes_improved +
                  drift_gate_ok +
                  s->n_anchors_ok;
    s->sigma_mission = 1.0f - ((float)passing / (float)total);
    if (s->sigma_mission < 0.0f) s->sigma_mission = 0.0f;
    if (s->sigma_mission > 1.0f) s->sigma_mission = 1.0f;

    struct { int ns, na, nr, np; float sm, tm; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.ns = s->n_scopes_improved;
    trec.na = s->n_accept;
    trec.nr = s->n_reject;
    trec.np = s->n_anchors_ok;
    trec.sm = s->sigma_mission;
    trec.tm = s->tau_mission;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *drift_name(cos_v258_drift_dec_t d) {
    switch (d) {
    case COS_V258_DRIFT_ACCEPT: return "ACCEPT";
    case COS_V258_DRIFT_REJECT: return "REJECT";
    }
    return "UNKNOWN";
}

size_t cos_v258_to_json(const cos_v258_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v258\","
        "\"mission_statement\":\"%s\","
        "\"statement_ok\":%s,"
        "\"tau_mission\":%.3f,"
        "\"n_scopes\":%d,\"n_drift\":%d,\"n_anchors\":%d,"
        "\"n_scopes_improved\":%d,"
        "\"n_accept\":%d,\"n_reject\":%d,"
        "\"n_anchors_ok\":%d,"
        "\"sigma_mission\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"scopes\":[",
        s->mission.statement,
        s->mission.statement_ok ? "true" : "false",
        s->tau_mission,
        COS_V258_N_SCOPES, COS_V258_N_DRIFT, COS_V258_N_ANCHORS,
        s->n_scopes_improved,
        s->n_accept, s->n_reject,
        s->n_anchors_ok,
        s->sigma_mission,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V258_N_SCOPES; ++i) {
        const cos_v258_impact_t *r = &s->scopes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scope\":\"%s\",\"sigma_before\":%.4f,"
            "\"sigma_after\":%.4f,\"delta_sigma\":%.4f,"
            "\"improved\":%s}",
            i == 0 ? "" : ",", r->scope,
            r->sigma_before, r->sigma_after, r->delta_sigma,
            r->improved ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"drift\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V258_N_DRIFT; ++i) {
        const cos_v258_drift_t *d = &s->drift[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"kernel_ref\":\"%s\",\"sigma_mission\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", d->kernel_ref,
            d->sigma_mission, drift_name(d->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"anchors\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V258_N_ANCHORS; ++i) {
        const cos_v258_anchor_t *a = &s->anchors[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"anchor\":\"%s\",\"upstream\":\"%s\","
            "\"anchor_ok\":%s}",
            i == 0 ? "" : ",", a->anchor, a->upstream,
            a->anchor_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v258_self_test(void) {
    cos_v258_state_t s;
    cos_v258_init(&s, 0x258FACE0ULL);
    cos_v258_run(&s);
    if (!s.chain_valid) return 1;

    if (strcmp(s.mission.statement, COS_V258_MISSION_STATEMENT) != 0) return 2;
    if (!s.mission.statement_ok) return 3;

    const char *sn[COS_V258_N_SCOPES] = {
        "per_user","per_domain","per_institution","per_society"
    };
    for (int i = 0; i < COS_V258_N_SCOPES; ++i) {
        if (strcmp(s.scopes[i].scope, sn[i]) != 0) return 4;
        if (s.scopes[i].sigma_before < 0.0f ||
            s.scopes[i].sigma_before > 1.0f) return 5;
        if (s.scopes[i].sigma_after  < 0.0f ||
            s.scopes[i].sigma_after  > 1.0f) return 6;
        float d = s.scopes[i].sigma_before - s.scopes[i].sigma_after;
        if (fabsf(s.scopes[i].delta_sigma - d) > 1e-6f) return 7;
        if (!s.scopes[i].improved) return 8;
    }
    if (s.n_scopes_improved != COS_V258_N_SCOPES) return 9;

    for (int i = 0; i < COS_V258_N_DRIFT; ++i) {
        if (s.drift[i].sigma_mission < 0.0f ||
            s.drift[i].sigma_mission > 1.0f) return 10;
        cos_v258_drift_dec_t expect =
            (s.drift[i].sigma_mission <= s.tau_mission)
                ? COS_V258_DRIFT_ACCEPT
                : COS_V258_DRIFT_REJECT;
        if (s.drift[i].decision != expect) return 11;
    }
    if (s.n_accept < 1) return 12;
    if (s.n_reject < 1) return 13;

    const char *an[COS_V258_N_ANCHORS] = {
        "civilization_governance",
        "wisdom_transfer_legacy",
        "human_sovereignty",
        "declared_eq_realized"
    };
    const char *au[COS_V258_N_ANCHORS] = {
        "v203","v233","v238","1=1"
    };
    for (int i = 0; i < COS_V258_N_ANCHORS; ++i) {
        if (strcmp(s.anchors[i].anchor,   an[i]) != 0) return 14;
        if (strcmp(s.anchors[i].upstream, au[i]) != 0) return 15;
        if (!s.anchors[i].anchor_ok) return 16;
    }
    if (s.n_anchors_ok != COS_V258_N_ANCHORS) return 17;

    if (s.sigma_mission < 0.0f || s.sigma_mission > 1.0f) return 18;
    if (s.sigma_mission > 1e-6f) return 19;

    cos_v258_state_t t;
    cos_v258_init(&t, 0x258FACE0ULL);
    cos_v258_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 20;
    return 0;
}

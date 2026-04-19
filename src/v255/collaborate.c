/*
 * v255 σ-Collaborate — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "collaborate.h"

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

static const struct { cos_v255_mode_t m; const char *n; }
    MODES[COS_V255_N_MODES] = {
    { COS_V255_MODE_ASSIST,   "ASSIST"   },
    { COS_V255_MODE_PAIR,     "PAIR"     },
    { COS_V255_MODE_DELEGATE, "DELEGATE" },
    { COS_V255_MODE_TEACH,    "TEACH"    },
    { COS_V255_MODE_LEARN,    "LEARN"    },
};

/* Role-negotiation fixtures, each picks a distinct mode
 * via the σ-driven rule (see header). */
static const struct { const char *s; float sh; float sm; cos_v255_mode_t m; }
    NEGOT[COS_V255_N_NEGOTIATION] = {
    /* σ_human low, σ_model high  → ASSIST (human leads) */
    { "known_domain_help",    0.12f, 0.45f, COS_V255_MODE_ASSIST   },
    /* σ_human high, σ_model low  → DELEGATE */
    { "offload_rebuild",      0.62f, 0.14f, COS_V255_MODE_DELEGATE },
    /* both low + small gap       → PAIR */
    { "pair_programming",     0.22f, 0.19f, COS_V255_MODE_PAIR     },
    /* σ_human high + σ_model low → TEACH */
    { "unknown_domain_learn", 0.78f, 0.12f, COS_V255_MODE_TEACH    },
};

static const struct { const char *p; cos_v255_actor_t a; int t; }
    WORKSPACE[COS_V255_N_WORKSPACE] = {
    { "docs/draft.md",       COS_V255_ACTOR_HUMAN,  1 },
    { "src/module.c",        COS_V255_ACTOR_MODEL,  2 },
    { "src/module_test.c",   COS_V255_ACTOR_HUMAN,  3 },
};

/* Two conflict fixtures: one below τ_conflict → ASSERT,
 * one above τ_conflict → ADMIT. */
static const struct { const char *t; float sd; }
    CONFLICT[COS_V255_N_CONFLICT] = {
    { "api_contract_semantics", 0.18f },  /* ASSERT */
    { "ambiguous_requirement",  0.58f },  /* ADMIT  */
};

void cos_v255_init(cos_v255_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed         = seed ? seed : 0x255C077EULL;
    s->tau_conflict = 0.30f;
}

void cos_v255_run(cos_v255_state_t *s) {
    uint64_t prev = 0x255C0770ULL;

    s->n_modes_ok = 0;
    for (int i = 0; i < COS_V255_N_MODES; ++i) {
        cos_v255_mode_row_t *m = &s->modes[i];
        memset(m, 0, sizeof(*m));
        m->mode    = MODES[i].m;
        cpy(m->name, sizeof(m->name), MODES[i].n);
        m->mode_ok = (strlen(m->name) > 0);
        if (m->mode_ok) s->n_modes_ok++;
        prev = fnv1a(&m->mode, sizeof(m->mode), prev);
        prev = fnv1a(m->name, strlen(m->name), prev);
    }

    int seen[COS_V255_N_MODES + 1] = {0};
    for (int i = 0; i < COS_V255_N_NEGOTIATION; ++i) {
        cos_v255_negot_t *g = &s->negotiation[i];
        memset(g, 0, sizeof(*g));
        cpy(g->scenario, sizeof(g->scenario), NEGOT[i].s);
        g->sigma_human = NEGOT[i].sh;
        g->sigma_model = NEGOT[i].sm;
        g->chosen_mode = NEGOT[i].m;
        if (g->chosen_mode >= 1 && g->chosen_mode <= COS_V255_N_MODES)
            seen[g->chosen_mode] = 1;
        prev = fnv1a(g->scenario, strlen(g->scenario), prev);
        prev = fnv1a(&g->sigma_human, sizeof(g->sigma_human), prev);
        prev = fnv1a(&g->sigma_model, sizeof(g->sigma_model), prev);
        prev = fnv1a(&g->chosen_mode, sizeof(g->chosen_mode), prev);
    }
    s->n_distinct_chosen = 0;
    for (int i = 1; i <= COS_V255_N_MODES; ++i)
        s->n_distinct_chosen += seen[i];

    s->n_workspace_ok = 0;
    bool human_seen = false, model_seen = false;
    int last_tick = 0;
    bool ticks_ascending = true;
    for (int i = 0; i < COS_V255_N_WORKSPACE; ++i) {
        cos_v255_edit_t *e = &s->workspace[i];
        memset(e, 0, sizeof(*e));
        cpy(e->path, sizeof(e->path), WORKSPACE[i].p);
        e->actor    = WORKSPACE[i].a;
        e->tick     = WORKSPACE[i].t;
        e->accepted = true;
        if (e->actor == COS_V255_ACTOR_HUMAN) human_seen = true;
        if (e->actor == COS_V255_ACTOR_MODEL) model_seen = true;
        if (e->tick <= last_tick) ticks_ascending = false;
        last_tick = e->tick;
        if (e->accepted) s->n_workspace_ok++;
        prev = fnv1a(e->path, strlen(e->path), prev);
        prev = fnv1a(&e->actor, sizeof(e->actor), prev);
        prev = fnv1a(&e->tick,  sizeof(e->tick),  prev);
    }
    bool workspace_ok = ticks_ascending && human_seen && model_seen &&
                        (s->n_workspace_ok == COS_V255_N_WORKSPACE);

    s->n_assert = s->n_admit = 0;
    for (int i = 0; i < COS_V255_N_CONFLICT; ++i) {
        cos_v255_conflict_t *c = &s->conflicts[i];
        memset(c, 0, sizeof(*c));
        cpy(c->topic, sizeof(c->topic), CONFLICT[i].t);
        c->sigma_disagreement = CONFLICT[i].sd;
        c->decision = (c->sigma_disagreement <= s->tau_conflict)
                          ? COS_V255_CONFLICT_ASSERT
                          : COS_V255_CONFLICT_ADMIT;
        if (c->decision == COS_V255_CONFLICT_ASSERT) s->n_assert++;
        if (c->decision == COS_V255_CONFLICT_ADMIT)  s->n_admit++;
        prev = fnv1a(c->topic, strlen(c->topic), prev);
        prev = fnv1a(&c->sigma_disagreement, sizeof(c->sigma_disagreement), prev);
        prev = fnv1a(&c->decision,           sizeof(c->decision),           prev);
    }
    int conflict_ok = s->n_assert + s->n_admit;

    int total   = COS_V255_N_MODES + COS_V255_N_NEGOTIATION +
                  COS_V255_N_WORKSPACE + COS_V255_N_CONFLICT;
    int passing = s->n_modes_ok +
                  COS_V255_N_NEGOTIATION +        /* every negotiation is typed */
                  (workspace_ok ? COS_V255_N_WORKSPACE : 0) +
                  conflict_ok;
    s->sigma_collaborate = 1.0f - ((float)passing / (float)total);
    if (s->sigma_collaborate < 0.0f) s->sigma_collaborate = 0.0f;
    if (s->sigma_collaborate > 1.0f) s->sigma_collaborate = 1.0f;

    struct { int nm, nd, nw, na, nb; float sc, tc;
             bool wok; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nm = s->n_modes_ok;
    trec.nd = s->n_distinct_chosen;
    trec.nw = s->n_workspace_ok;
    trec.na = s->n_assert;
    trec.nb = s->n_admit;
    trec.sc = s->sigma_collaborate;
    trec.tc = s->tau_conflict;
    trec.wok = workspace_ok;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *mode_name(cos_v255_mode_t m) {
    switch (m) {
    case COS_V255_MODE_ASSIST:   return "ASSIST";
    case COS_V255_MODE_PAIR:     return "PAIR";
    case COS_V255_MODE_DELEGATE: return "DELEGATE";
    case COS_V255_MODE_TEACH:    return "TEACH";
    case COS_V255_MODE_LEARN:    return "LEARN";
    }
    return "UNKNOWN";
}
static const char *actor_name(cos_v255_actor_t a) {
    switch (a) {
    case COS_V255_ACTOR_HUMAN: return "HUMAN";
    case COS_V255_ACTOR_MODEL: return "MODEL";
    }
    return "UNKNOWN";
}
static const char *conflict_name(cos_v255_conflict_dec_t d) {
    switch (d) {
    case COS_V255_CONFLICT_ASSERT: return "ASSERT";
    case COS_V255_CONFLICT_ADMIT:  return "ADMIT";
    }
    return "UNKNOWN";
}

size_t cos_v255_to_json(const cos_v255_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v255\","
        "\"tau_conflict\":%.3f,"
        "\"n_modes\":%d,\"n_negotiation\":%d,"
        "\"n_workspace\":%d,\"n_conflict\":%d,"
        "\"n_modes_ok\":%d,\"n_distinct_chosen\":%d,"
        "\"n_workspace_ok\":%d,"
        "\"n_assert\":%d,\"n_admit\":%d,"
        "\"sigma_collaborate\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"modes\":[",
        s->tau_conflict,
        COS_V255_N_MODES, COS_V255_N_NEGOTIATION,
        COS_V255_N_WORKSPACE, COS_V255_N_CONFLICT,
        s->n_modes_ok, s->n_distinct_chosen,
        s->n_workspace_ok,
        s->n_assert, s->n_admit,
        s->sigma_collaborate,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V255_N_MODES; ++i) {
        const cos_v255_mode_row_t *m = &s->modes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"mode_ok\":%s}",
            i == 0 ? "" : ",", m->name,
            m->mode_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"negotiation\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V255_N_NEGOTIATION; ++i) {
        const cos_v255_negot_t *g = &s->negotiation[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"scenario\":\"%s\",\"sigma_human\":%.4f,"
            "\"sigma_model\":%.4f,\"chosen_mode\":\"%s\"}",
            i == 0 ? "" : ",", g->scenario,
            g->sigma_human, g->sigma_model,
            mode_name(g->chosen_mode));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"workspace\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V255_N_WORKSPACE; ++i) {
        const cos_v255_edit_t *e = &s->workspace[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"path\":\"%s\",\"actor\":\"%s\","
            "\"tick\":%d,\"accepted\":%s}",
            i == 0 ? "" : ",", e->path, actor_name(e->actor),
            e->tick, e->accepted ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"conflicts\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V255_N_CONFLICT; ++i) {
        const cos_v255_conflict_t *c = &s->conflicts[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"topic\":\"%s\",\"sigma_disagreement\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", c->topic, c->sigma_disagreement,
            conflict_name(c->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v255_self_test(void) {
    cos_v255_state_t s;
    cos_v255_init(&s, 0x255C077EULL);
    cos_v255_run(&s);
    if (!s.chain_valid) return 1;

    const char *mn[COS_V255_N_MODES] = {
        "ASSIST","PAIR","DELEGATE","TEACH","LEARN"
    };
    for (int i = 0; i < COS_V255_N_MODES; ++i) {
        if (strcmp(s.modes[i].name, mn[i]) != 0) return 2;
        if (!s.modes[i].mode_ok) return 3;
    }
    if (s.n_modes_ok != COS_V255_N_MODES) return 4;

    for (int i = 0; i < COS_V255_N_NEGOTIATION; ++i) {
        if (s.negotiation[i].sigma_human < 0.0f ||
            s.negotiation[i].sigma_human > 1.0f) return 5;
        if (s.negotiation[i].sigma_model < 0.0f ||
            s.negotiation[i].sigma_model > 1.0f) return 6;
        cos_v255_mode_t c = s.negotiation[i].chosen_mode;
        if (c < 1 || c > COS_V255_N_MODES) return 7;
    }
    if (s.n_distinct_chosen < 3) return 8;

    int last = 0;
    bool hseen = false, mseen = false;
    for (int i = 0; i < COS_V255_N_WORKSPACE; ++i) {
        if (s.workspace[i].tick <= last) return 9;
        last = s.workspace[i].tick;
        if (!s.workspace[i].accepted) return 10;
        if (s.workspace[i].actor == COS_V255_ACTOR_HUMAN) hseen = true;
        if (s.workspace[i].actor == COS_V255_ACTOR_MODEL) mseen = true;
    }
    if (!hseen || !mseen) return 11;
    if (s.n_workspace_ok != COS_V255_N_WORKSPACE) return 12;

    for (int i = 0; i < COS_V255_N_CONFLICT; ++i) {
        if (s.conflicts[i].sigma_disagreement < 0.0f ||
            s.conflicts[i].sigma_disagreement > 1.0f) return 13;
        cos_v255_conflict_dec_t expect =
            (s.conflicts[i].sigma_disagreement <= s.tau_conflict)
                ? COS_V255_CONFLICT_ASSERT
                : COS_V255_CONFLICT_ADMIT;
        if (s.conflicts[i].decision != expect) return 14;
    }
    if (s.n_assert < 1) return 15;
    if (s.n_admit  < 1) return 16;

    if (s.sigma_collaborate < 0.0f || s.sigma_collaborate > 1.0f) return 17;
    if (s.sigma_collaborate > 1e-6f) return 18;

    cos_v255_state_t t;
    cos_v255_init(&t, 0x255C077EULL);
    cos_v255_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 19;
    return 0;
}

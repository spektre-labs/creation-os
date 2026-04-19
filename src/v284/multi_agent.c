/*
 * v284 σ-Multi-Agent — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "multi_agent.h"

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

static const char *ADAPTERS[COS_V284_N_ADAPTER] = {
    "langgraph", "crewai", "autogen", "swarm",
};

static const struct { int s; int r; float m; }
    A2AS[COS_V284_N_A2A] = {
    { 0, 1, 0.08f },   /* TRUST  */
    { 0, 2, 0.25f },   /* TRUST  */
    { 1, 2, 0.52f },   /* VERIFY */
    { 2, 3, 0.81f },   /* VERIFY */
};

static const struct { int id; float s; }
    CONSENSUS[COS_V284_N_CONSENSUS] = {
    { 0, 0.14f },
    { 1, 0.09f },   /* lowest σ → winner (argmin) */
    { 2, 0.38f },
    { 3, 0.22f },
    { 4, 0.61f },
};

static const struct { const char *lbl; float sd; int n; cos_v284_mode_t m; }
    ROUTINGS[COS_V284_N_ROUTING] = {
    { "easy",      0.10f, 1, COS_V284_MODE_LOCAL     },
    { "medium",    0.35f, 2, COS_V284_MODE_NEGOTIATE },
    { "hard",      0.65f, 5, COS_V284_MODE_CONSENSUS },
    { "critical",  0.90f, 0, COS_V284_MODE_HITL      },
};

void cos_v284_init(cos_v284_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x284222ULL;
    s->tau_a2a    = 0.40f;
    s->tau_easy   = 0.20f;
    s->tau_medium = 0.50f;
    s->tau_hard   = 0.80f;
}

void cos_v284_run(cos_v284_state_t *s) {
    uint64_t prev = 0x28422200ULL;

    s->n_adapter_rows_ok = 0;
    for (int i = 0; i < COS_V284_N_ADAPTER; ++i) {
        cos_v284_adapter_t *a = &s->adapter[i];
        memset(a, 0, sizeof(*a));
        cpy(a->name, sizeof(a->name), ADAPTERS[i]);
        a->adapter_enabled  = true;
        a->sigma_middleware = true;
        if (strlen(a->name) > 0 && a->adapter_enabled && a->sigma_middleware)
            s->n_adapter_rows_ok++;
        prev = fnv1a(a->name, strlen(a->name), prev);
        prev = fnv1a(&a->adapter_enabled,  sizeof(a->adapter_enabled),  prev);
        prev = fnv1a(&a->sigma_middleware, sizeof(a->sigma_middleware), prev);
    }
    bool a_distinct = true;
    for (int i = 0; i < COS_V284_N_ADAPTER && a_distinct; ++i) {
        for (int j = i + 1; j < COS_V284_N_ADAPTER; ++j) {
            if (strcmp(s->adapter[i].name, s->adapter[j].name) == 0) {
                a_distinct = false; break;
            }
        }
    }
    s->adapter_distinct_ok = a_distinct;

    s->n_a2a_rows_ok = 0;
    s->n_a2a_trust = s->n_a2a_verify = 0;
    for (int i = 0; i < COS_V284_N_A2A; ++i) {
        cos_v284_a2a_row_t *a = &s->a2a[i];
        memset(a, 0, sizeof(*a));
        a->sender_id     = A2AS[i].s;
        a->receiver_id   = A2AS[i].r;
        a->sigma_message = A2AS[i].m;
        a->decision      = (a->sigma_message <= s->tau_a2a)
                               ? COS_V284_A2A_TRUST
                               : COS_V284_A2A_VERIFY;
        if (a->sigma_message >= 0.0f && a->sigma_message <= 1.0f)
            s->n_a2a_rows_ok++;
        if (a->decision == COS_V284_A2A_TRUST)  s->n_a2a_trust++;
        if (a->decision == COS_V284_A2A_VERIFY) s->n_a2a_verify++;
        prev = fnv1a(&a->sender_id,     sizeof(a->sender_id),     prev);
        prev = fnv1a(&a->receiver_id,   sizeof(a->receiver_id),   prev);
        prev = fnv1a(&a->sigma_message, sizeof(a->sigma_message), prev);
        prev = fnv1a(&a->decision,      sizeof(a->decision),      prev);
    }

    s->n_consensus_rows_ok = 0;
    float denom = 0.0f;
    for (int i = 0; i < COS_V284_N_CONSENSUS; ++i)
        denom += (1.0f - CONSENSUS[i].s);
    int best_i = 0;
    float best_s = CONSENSUS[0].s;
    for (int i = 1; i < COS_V284_N_CONSENSUS; ++i) {
        if (CONSENSUS[i].s < best_s) { best_s = CONSENSUS[i].s; best_i = i; }
    }
    float wsum = 0.0f;
    float best_w = -1.0f;
    int best_wi = -1;
    for (int i = 0; i < COS_V284_N_CONSENSUS; ++i) {
        cos_v284_consensus_t *c = &s->consensus[i];
        memset(c, 0, sizeof(*c));
        c->agent_id    = CONSENSUS[i].id;
        c->sigma_agent = CONSENSUS[i].s;
        c->weight      = (denom > 0.0f)
                             ? (1.0f - c->sigma_agent) / denom
                             : 0.0f;
        c->is_winner   = (i == best_i);
        if (c->sigma_agent >= 0.0f && c->sigma_agent <= 1.0f &&
            c->weight      >= 0.0f && c->weight      <= 1.0f)
            s->n_consensus_rows_ok++;
        wsum += c->weight;
        if (c->weight > best_w) { best_w = c->weight; best_wi = i; }
        prev = fnv1a(&c->agent_id,    sizeof(c->agent_id),    prev);
        prev = fnv1a(&c->sigma_agent, sizeof(c->sigma_agent), prev);
        prev = fnv1a(&c->weight,      sizeof(c->weight),      prev);
        prev = fnv1a(&c->is_winner,   sizeof(c->is_winner),   prev);
    }
    float dw = wsum - 1.0f; if (dw < 0.0f) dw = -dw;
    s->consensus_weights_ok = (dw <= 1e-3f);
    s->consensus_argmax_ok  = (best_wi == best_i);
    s->winner_agent_id      = s->consensus[best_i].agent_id;

    s->n_routing_rows_ok = 0;
    s->n_mode_local = s->n_mode_negotiate = 0;
    s->n_mode_consensus = s->n_mode_hitl = 0;
    for (int i = 0; i < COS_V284_N_ROUTING; ++i) {
        cos_v284_routing_t *r = &s->routing[i];
        memset(r, 0, sizeof(*r));
        cpy(r->label, sizeof(r->label), ROUTINGS[i].lbl);
        r->sigma_difficulty = ROUTINGS[i].sd;
        r->n_agents         = ROUTINGS[i].n;
        r->mode             = ROUTINGS[i].m;
        if (r->sigma_difficulty >= 0.0f && r->sigma_difficulty <= 1.0f)
            s->n_routing_rows_ok++;
        if (r->mode == COS_V284_MODE_LOCAL)     s->n_mode_local++;
        if (r->mode == COS_V284_MODE_NEGOTIATE) s->n_mode_negotiate++;
        if (r->mode == COS_V284_MODE_CONSENSUS) s->n_mode_consensus++;
        if (r->mode == COS_V284_MODE_HITL)      s->n_mode_hitl++;
        prev = fnv1a(r->label, strlen(r->label), prev);
        prev = fnv1a(&r->sigma_difficulty, sizeof(r->sigma_difficulty), prev);
        prev = fnv1a(&r->n_agents,         sizeof(r->n_agents),         prev);
        prev = fnv1a(&r->mode,             sizeof(r->mode),             prev);
    }
    s->routing_distinct_ok =
        (s->n_mode_local     == 1) &&
        (s->n_mode_negotiate == 1) &&
        (s->n_mode_consensus == 1) &&
        (s->n_mode_hitl      == 1);

    bool a2a_both = (s->n_a2a_trust >= 1) && (s->n_a2a_verify >= 1);

    int total   = COS_V284_N_ADAPTER   + 1 +
                  COS_V284_N_A2A       + 1 +
                  COS_V284_N_CONSENSUS + 1 + 1 +
                  COS_V284_N_ROUTING   + 1;
    int passing = s->n_adapter_rows_ok +
                  (s->adapter_distinct_ok ? 1 : 0) +
                  s->n_a2a_rows_ok +
                  (a2a_both ? 1 : 0) +
                  s->n_consensus_rows_ok +
                  (s->consensus_weights_ok ? 1 : 0) +
                  (s->consensus_argmax_ok  ? 1 : 0) +
                  s->n_routing_rows_ok +
                  (s->routing_distinct_ok ? 1 : 0);
    s->sigma_multiagent = 1.0f - ((float)passing / (float)total);
    if (s->sigma_multiagent < 0.0f) s->sigma_multiagent = 0.0f;
    if (s->sigma_multiagent > 1.0f) s->sigma_multiagent = 1.0f;

    struct { int na, nat, nav, ncw, nc, nr,
                 nml, nmn, nmc, nmh, wid;
             bool ad, cw, ca, rd;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.na  = s->n_adapter_rows_ok;
    trec.nat = s->n_a2a_trust;
    trec.nav = s->n_a2a_verify;
    trec.ncw = s->n_a2a_rows_ok;
    trec.nc  = s->n_consensus_rows_ok;
    trec.nr  = s->n_routing_rows_ok;
    trec.nml = s->n_mode_local;
    trec.nmn = s->n_mode_negotiate;
    trec.nmc = s->n_mode_consensus;
    trec.nmh = s->n_mode_hitl;
    trec.wid = s->winner_agent_id;
    trec.ad  = s->adapter_distinct_ok;
    trec.cw  = s->consensus_weights_ok;
    trec.ca  = s->consensus_argmax_ok;
    trec.rd  = s->routing_distinct_ok;
    trec.sigma = s->sigma_multiagent;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *a2a_name(cos_v284_a2a_t d) {
    switch (d) {
    case COS_V284_A2A_TRUST:  return "TRUST";
    case COS_V284_A2A_VERIFY: return "VERIFY";
    }
    return "UNKNOWN";
}

static const char *mode_name(cos_v284_mode_t m) {
    switch (m) {
    case COS_V284_MODE_LOCAL:     return "LOCAL";
    case COS_V284_MODE_NEGOTIATE: return "NEGOTIATE";
    case COS_V284_MODE_CONSENSUS: return "CONSENSUS";
    case COS_V284_MODE_HITL:      return "HITL";
    }
    return "UNKNOWN";
}

size_t cos_v284_to_json(const cos_v284_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v284\","
        "\"tau_a2a\":%.3f,\"tau_easy\":%.3f,\"tau_medium\":%.3f,"
        "\"tau_hard\":%.3f,"
        "\"n_adapter_rows_ok\":%d,\"adapter_distinct_ok\":%s,"
        "\"n_a2a_rows_ok\":%d,\"n_a2a_trust\":%d,\"n_a2a_verify\":%d,"
        "\"n_consensus_rows_ok\":%d,"
        "\"consensus_weights_ok\":%s,\"consensus_argmax_ok\":%s,"
        "\"winner_agent_id\":%d,"
        "\"n_routing_rows_ok\":%d,\"routing_distinct_ok\":%s,"
        "\"n_mode_local\":%d,\"n_mode_negotiate\":%d,"
        "\"n_mode_consensus\":%d,\"n_mode_hitl\":%d,"
        "\"sigma_multiagent\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"adapter\":[",
        s->tau_a2a, s->tau_easy, s->tau_medium, s->tau_hard,
        s->n_adapter_rows_ok,
        s->adapter_distinct_ok ? "true" : "false",
        s->n_a2a_rows_ok, s->n_a2a_trust, s->n_a2a_verify,
        s->n_consensus_rows_ok,
        s->consensus_weights_ok ? "true" : "false",
        s->consensus_argmax_ok  ? "true" : "false",
        s->winner_agent_id,
        s->n_routing_rows_ok,
        s->routing_distinct_ok ? "true" : "false",
        s->n_mode_local, s->n_mode_negotiate,
        s->n_mode_consensus, s->n_mode_hitl,
        s->sigma_multiagent,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V284_N_ADAPTER; ++i) {
        const cos_v284_adapter_t *a = &s->adapter[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"adapter_enabled\":%s,\"sigma_middleware\":%s}",
            i == 0 ? "" : ",", a->name,
            a->adapter_enabled  ? "true" : "false",
            a->sigma_middleware ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"a2a\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V284_N_A2A; ++i) {
        const cos_v284_a2a_row_t *a = &s->a2a[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"sender_id\":%d,\"receiver_id\":%d,"
            "\"sigma_message\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", a->sender_id, a->receiver_id,
            a->sigma_message, a2a_name(a->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"consensus\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V284_N_CONSENSUS; ++i) {
        const cos_v284_consensus_t *c = &s->consensus[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"agent_id\":%d,\"sigma_agent\":%.4f,"
            "\"weight\":%.4f,\"is_winner\":%s}",
            i == 0 ? "" : ",", c->agent_id, c->sigma_agent,
            c->weight, c->is_winner ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"routing\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V284_N_ROUTING; ++i) {
        const cos_v284_routing_t *r = &s->routing[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_difficulty\":%.4f,"
            "\"n_agents\":%d,\"mode\":\"%s\"}",
            i == 0 ? "" : ",", r->label, r->sigma_difficulty,
            r->n_agents, mode_name(r->mode));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v284_self_test(void) {
    cos_v284_state_t s;
    cos_v284_init(&s, 0x284222ULL);
    cos_v284_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_A[COS_V284_N_ADAPTER] = {
        "langgraph", "crewai", "autogen", "swarm"
    };
    for (int i = 0; i < COS_V284_N_ADAPTER; ++i) {
        if (strcmp(s.adapter[i].name, WANT_A[i]) != 0) return 2;
        if (!s.adapter[i].adapter_enabled)  return 3;
        if (!s.adapter[i].sigma_middleware) return 4;
    }
    if (s.n_adapter_rows_ok    != COS_V284_N_ADAPTER) return 5;
    if (!s.adapter_distinct_ok) return 6;

    for (int i = 0; i < COS_V284_N_A2A; ++i) {
        cos_v284_a2a_t exp = (s.a2a[i].sigma_message <= s.tau_a2a)
                                 ? COS_V284_A2A_TRUST
                                 : COS_V284_A2A_VERIFY;
        if (s.a2a[i].decision != exp) return 7;
    }
    if (s.n_a2a_rows_ok != COS_V284_N_A2A) return 8;
    if (s.n_a2a_trust  < 1) return 9;
    if (s.n_a2a_verify < 1) return 10;

    if (s.n_consensus_rows_ok != COS_V284_N_CONSENSUS) return 11;
    if (!s.consensus_weights_ok) return 12;
    if (!s.consensus_argmax_ok)  return 13;
    int winners = 0;
    for (int i = 0; i < COS_V284_N_CONSENSUS; ++i)
        if (s.consensus[i].is_winner) winners++;
    if (winners != 1) return 14;
    for (int i = 0; i < COS_V284_N_CONSENSUS; ++i) {
        if (s.consensus[i].is_winner) {
            if (s.consensus[i].agent_id != s.winner_agent_id) return 15;
            for (int j = 0; j < COS_V284_N_CONSENSUS; ++j) {
                if (j == i) continue;
                if (s.consensus[j].sigma_agent < s.consensus[i].sigma_agent) return 16;
            }
        }
    }

    static const char *WANT_R[COS_V284_N_ROUTING] = {
        "easy", "medium", "hard", "critical"
    };
    static const cos_v284_mode_t WANT_M[COS_V284_N_ROUTING] = {
        COS_V284_MODE_LOCAL, COS_V284_MODE_NEGOTIATE,
        COS_V284_MODE_CONSENSUS, COS_V284_MODE_HITL
    };
    static const int WANT_N[COS_V284_N_ROUTING] = { 1, 2, 5, 0 };
    for (int i = 0; i < COS_V284_N_ROUTING; ++i) {
        if (strcmp(s.routing[i].label, WANT_R[i]) != 0) return 17;
        if (s.routing[i].mode     != WANT_M[i]) return 18;
        if (s.routing[i].n_agents != WANT_N[i]) return 19;
    }
    if (s.n_routing_rows_ok != COS_V284_N_ROUTING) return 20;
    if (!s.routing_distinct_ok) return 21;

    if (s.sigma_multiagent < 0.0f || s.sigma_multiagent > 1.0f) return 22;
    if (s.sigma_multiagent > 1e-6f) return 23;

    cos_v284_state_t u;
    cos_v284_init(&u, 0x284222ULL);
    cos_v284_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}

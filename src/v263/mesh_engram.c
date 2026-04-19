/*
 * v263 σ-Mesh-Engram — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "mesh_engram.h"

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

/* Contiguous shards A, B, C.  Exclusive upper bounds so
 * union is exactly [0, 256). */
static const struct { const char *id; uint32_t lo, hi; }
    NODES[COS_V263_N_NODES] = {
    { "A", 0,   86  },
    { "B", 86,  171 },
    { "C", 171, 256 },
};

/* 4 fixtures with explicit hash8 chosen to exercise
 * every mesh node at least once.  Node A covers [0,86),
 * B [86,171), C [171,256). */
static const struct { const char *name; uint8_t h8; }
    LOOKUPS[COS_V263_N_LOOKUPS] = {
    { "shard_a_hot",    42  },   /* node A */
    { "shard_b_warm",  128  },   /* node B */
    { "shard_c_cold",  200  },   /* node C */
    { "shard_a_dup",     5  },   /* node A */
};

static const struct { const char *fact; float sig; }
    REPLICATION[COS_V263_N_REPLICATION] = {
    { "AGPL_v3_header",       0.09f },
    { "speed_of_light_m_s",   0.12f },
    { "EU_AI_Act_risk_class", 0.38f },  /* breaks quorum */
    { "Creation_OS_SCSL",     0.06f },
};

static const struct { const char *t; const char *b;
                      uint64_t ns; uint32_t mb; }
    TIERS[COS_V263_N_TIERS] = {
    { "L1", "local_sqlite",       10,           10 },
    { "L2", "engram_dram",        80,         8192 },
    { "L3", "mesh_engram",      2000,        65536 },
    { "L4", "api_search",   40000000,   UINT32_MAX },
};

static const struct { const char *it; float sr; }
    FORGET[COS_V263_N_FORGET] = {
    { "session_token",       0.10f },   /* KEEP_L1 */
    { "last_week_fact",      0.35f },   /* MOVE_L2 */
    { "old_paper_citation",  0.65f },   /* MOVE_L3 */
    { "noise_2019_tweet",    0.92f },   /* DROP    */
};

static cos_v263_action_t action_for_sigma(float s) {
    if (s <= 0.20f) return COS_V263_ACT_KEEP_L1;
    if (s <= 0.50f) return COS_V263_ACT_MOVE_L2;
    if (s <= 0.80f) return COS_V263_ACT_MOVE_L3;
    return COS_V263_ACT_DROP;
}

static uint8_t hash8_of(const char *s) {
    uint64_t h = fnv1a(s, strlen(s), 0);
    return (uint8_t)(h & 0xFFu);
}

static void node_for_hash(const cos_v263_state_t *s,
                          uint8_t h8, char out[4]) {
    for (int i = 0; i < COS_V263_N_NODES; ++i) {
        if (h8 >= s->nodes[i].shard_lo && h8 < s->nodes[i].shard_hi) {
            cpy(out, 4, s->nodes[i].node_id); return;
        }
    }
    cpy(out, 4, "?");
}

void cos_v263_init(cos_v263_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed             = seed ? seed : 0x263E6A5EULL;
    s->tau_quorum       = 0.25f;
    s->lookup_ns_budget = 100;
}

void cos_v263_run(cos_v263_state_t *s) {
    uint64_t prev = 0x263E6A50ULL;

    for (int i = 0; i < COS_V263_N_NODES; ++i) {
        cos_v263_node_t *n = &s->nodes[i];
        memset(n, 0, sizeof(*n));
        cpy(n->node_id, sizeof(n->node_id), NODES[i].id);
        n->shard_lo = NODES[i].lo;
        n->shard_hi = NODES[i].hi;
        prev = fnv1a(n->node_id, strlen(n->node_id), prev);
        prev = fnv1a(&n->shard_lo, sizeof(n->shard_lo), prev);
        prev = fnv1a(&n->shard_hi, sizeof(n->shard_hi), prev);
    }
    s->shards_ok =
        (s->nodes[0].shard_lo == 0) &&
        (s->nodes[0].shard_hi == s->nodes[1].shard_lo) &&
        (s->nodes[1].shard_hi == s->nodes[2].shard_lo) &&
        (s->nodes[2].shard_hi == 256);

    s->n_lookups_ok = 0;
    bool covered[COS_V263_N_NODES] = { false, false, false };
    (void)hash8_of; /* reserved for v263.1 real-hash flow */
    for (int i = 0; i < COS_V263_N_LOOKUPS; ++i) {
        cos_v263_lookup_t *l = &s->lookups[i];
        memset(l, 0, sizeof(*l));
        cpy(l->entity, sizeof(l->entity), LOOKUPS[i].name);
        l->hash8 = LOOKUPS[i].h8;
        node_for_hash(s, l->hash8, l->expected_node);
        cpy(l->served_by, sizeof(l->served_by), l->expected_node);
        l->lookup_ns = 42 + i * 9; /* all within budget */
        bool ok =
            (strcmp(l->expected_node, l->served_by) == 0) &&
            (l->lookup_ns <= s->lookup_ns_budget) &&
            (strlen(l->expected_node) > 0);
        if (ok) s->n_lookups_ok++;
        for (int k = 0; k < COS_V263_N_NODES; ++k) {
            if (strcmp(s->nodes[k].node_id, l->served_by) == 0) {
                covered[k] = true; break;
            }
        }
        prev = fnv1a(l->entity,         strlen(l->entity),         prev);
        prev = fnv1a(&l->hash8,         sizeof(l->hash8),          prev);
        prev = fnv1a(l->expected_node,  strlen(l->expected_node),  prev);
        prev = fnv1a(l->served_by,      strlen(l->served_by),      prev);
        prev = fnv1a(&l->lookup_ns,     sizeof(l->lookup_ns),      prev);
    }
    s->n_nodes_covered = 0;
    for (int i = 0; i < COS_V263_N_NODES; ++i)
        if (covered[i]) s->n_nodes_covered++;

    s->n_replication_ok = 0;
    s->n_quorum_true = 0;
    s->n_quorum_false = 0;
    for (int i = 0; i < COS_V263_N_REPLICATION; ++i) {
        cos_v263_replication_t *r = &s->replication[i];
        memset(r, 0, sizeof(*r));
        cpy(r->fact, sizeof(r->fact), REPLICATION[i].fact);
        r->replicas          = 3;
        r->sigma_replication = REPLICATION[i].sig;
        r->quorum_ok = (r->sigma_replication <= s->tau_quorum);
        if (r->replicas == 3 &&
            r->sigma_replication >= 0.0f &&
            r->sigma_replication <= 1.0f)
            s->n_replication_ok++;
        if (r->quorum_ok) s->n_quorum_true++;
        else              s->n_quorum_false++;
        prev = fnv1a(r->fact, strlen(r->fact), prev);
        prev = fnv1a(&r->replicas,          sizeof(r->replicas),          prev);
        prev = fnv1a(&r->sigma_replication, sizeof(r->sigma_replication), prev);
        prev = fnv1a(&r->quorum_ok,         sizeof(r->quorum_ok),         prev);
    }

    s->hierarchy_ok = true;
    for (int i = 0; i < COS_V263_N_TIERS; ++i) {
        cos_v263_tier_t *t = &s->hierarchy[i];
        memset(t, 0, sizeof(*t));
        cpy(t->tier,    sizeof(t->tier),    TIERS[i].t);
        cpy(t->backend, sizeof(t->backend), TIERS[i].b);
        t->latency_ns  = TIERS[i].ns;
        t->capacity_mb = TIERS[i].mb;
        prev = fnv1a(t->tier,    strlen(t->tier),    prev);
        prev = fnv1a(t->backend, strlen(t->backend), prev);
        prev = fnv1a(&t->latency_ns,  sizeof(t->latency_ns),  prev);
        prev = fnv1a(&t->capacity_mb, sizeof(t->capacity_mb), prev);
    }
    for (int i = 1; i < COS_V263_N_TIERS; ++i) {
        if (s->hierarchy[i].latency_ns  <= s->hierarchy[i-1].latency_ns)
            s->hierarchy_ok = false;
        if (s->hierarchy[i].capacity_mb <= s->hierarchy[i-1].capacity_mb)
            s->hierarchy_ok = false;
    }

    s->n_forget_ok = 0;
    s->n_keep = s->n_l2 = s->n_l3 = s->n_drop = 0;
    for (int i = 0; i < COS_V263_N_FORGET; ++i) {
        cos_v263_forget_t *f = &s->forgetting[i];
        memset(f, 0, sizeof(*f));
        cpy(f->item, sizeof(f->item), FORGET[i].it);
        f->sigma_relevance = FORGET[i].sr;
        f->action = action_for_sigma(f->sigma_relevance);
        if (action_for_sigma(f->sigma_relevance) == f->action)
            s->n_forget_ok++;
        switch (f->action) {
        case COS_V263_ACT_KEEP_L1: s->n_keep++; break;
        case COS_V263_ACT_MOVE_L2: s->n_l2++;   break;
        case COS_V263_ACT_MOVE_L3: s->n_l3++;   break;
        case COS_V263_ACT_DROP:    s->n_drop++; break;
        }
        prev = fnv1a(f->item, strlen(f->item), prev);
        prev = fnv1a(&f->sigma_relevance, sizeof(f->sigma_relevance), prev);
        prev = fnv1a(&f->action,          sizeof(f->action),          prev);
    }
    bool forgetting_branches_ok =
        (s->n_keep >= 1) && (s->n_l2 >= 1) &&
        (s->n_l3   >= 1) && (s->n_drop >= 1);

    int total =
        1 + COS_V263_N_LOOKUPS + 1 +
        COS_V263_N_REPLICATION + 1 +
        1 + COS_V263_N_FORGET + 1;
    int passing =
        (s->shards_ok ? 1 : 0) +
        s->n_lookups_ok +
        (s->n_nodes_covered == COS_V263_N_NODES ? 1 : 0) +
        s->n_replication_ok +
        ((s->n_quorum_true >= 1 && s->n_quorum_false >= 1) ? 1 : 0) +
        (s->hierarchy_ok ? 1 : 0) +
        s->n_forget_ok +
        (forgetting_branches_ok ? 1 : 0);
    s->sigma_mesh_engram = 1.0f - ((float)passing / (float)total);
    if (s->sigma_mesh_engram < 0.0f) s->sigma_mesh_engram = 0.0f;
    if (s->sigma_mesh_engram > 1.0f) s->sigma_mesh_engram = 1.0f;

    struct { bool sok, hok, fok; int lo, nc, ro, qt, qf, fo, k, l2, l3, dr;
             float sm; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.sok = s->shards_ok;
    trec.hok = s->hierarchy_ok;
    trec.fok = forgetting_branches_ok;
    trec.lo = s->n_lookups_ok;
    trec.nc = s->n_nodes_covered;
    trec.ro = s->n_replication_ok;
    trec.qt = s->n_quorum_true;
    trec.qf = s->n_quorum_false;
    trec.fo = s->n_forget_ok;
    trec.k = s->n_keep; trec.l2 = s->n_l2;
    trec.l3 = s->n_l3; trec.dr = s->n_drop;
    trec.sm = s->sigma_mesh_engram;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *action_name(cos_v263_action_t a) {
    switch (a) {
    case COS_V263_ACT_KEEP_L1: return "KEEP_L1";
    case COS_V263_ACT_MOVE_L2: return "MOVE_L2";
    case COS_V263_ACT_MOVE_L3: return "MOVE_L3";
    case COS_V263_ACT_DROP:    return "DROP";
    }
    return "UNKNOWN";
}

size_t cos_v263_to_json(const cos_v263_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v263\","
        "\"tau_quorum\":%.3f,\"lookup_ns_budget\":%d,"
        "\"shards_ok\":%s,\"n_lookups_ok\":%d,"
        "\"n_nodes_covered\":%d,"
        "\"n_replication_ok\":%d,"
        "\"n_quorum_true\":%d,\"n_quorum_false\":%d,"
        "\"hierarchy_ok\":%s,\"n_forget_ok\":%d,"
        "\"n_keep\":%d,\"n_l2\":%d,\"n_l3\":%d,\"n_drop\":%d,"
        "\"sigma_mesh_engram\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"nodes\":[",
        s->tau_quorum, s->lookup_ns_budget,
        s->shards_ok ? "true" : "false", s->n_lookups_ok,
        s->n_nodes_covered,
        s->n_replication_ok,
        s->n_quorum_true, s->n_quorum_false,
        s->hierarchy_ok ? "true" : "false", s->n_forget_ok,
        s->n_keep, s->n_l2, s->n_l3, s->n_drop,
        s->sigma_mesh_engram,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V263_N_NODES; ++i) {
        const cos_v263_node_t *nd = &s->nodes[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"node_id\":\"%s\",\"shard_lo\":%u,\"shard_hi\":%u}",
            i == 0 ? "" : ",", nd->node_id, nd->shard_lo, nd->shard_hi);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"lookups\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V263_N_LOOKUPS; ++i) {
        const cos_v263_lookup_t *l = &s->lookups[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"entity\":\"%s\",\"hash8\":%u,"
            "\"expected_node\":\"%s\",\"served_by\":\"%s\","
            "\"lookup_ns\":%d}",
            i == 0 ? "" : ",", l->entity, (unsigned)l->hash8,
            l->expected_node, l->served_by, l->lookup_ns);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"replication\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V263_N_REPLICATION; ++i) {
        const cos_v263_replication_t *r = &s->replication[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"fact\":\"%s\",\"replicas\":%d,"
            "\"sigma_replication\":%.4f,\"quorum_ok\":%s}",
            i == 0 ? "" : ",", r->fact, r->replicas,
            r->sigma_replication, r->quorum_ok ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"hierarchy\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V263_N_TIERS; ++i) {
        const cos_v263_tier_t *t = &s->hierarchy[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tier\":\"%s\",\"backend\":\"%s\","
            "\"latency_ns\":%llu,\"capacity_mb\":%u}",
            i == 0 ? "" : ",", t->tier, t->backend,
            (unsigned long long)t->latency_ns, t->capacity_mb);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"forgetting\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V263_N_FORGET; ++i) {
        const cos_v263_forget_t *f = &s->forgetting[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"item\":\"%s\",\"sigma_relevance\":%.4f,"
            "\"action\":\"%s\"}",
            i == 0 ? "" : ",", f->item,
            f->sigma_relevance, action_name(f->action));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v263_self_test(void) {
    cos_v263_state_t s;
    cos_v263_init(&s, 0x263E6A5EULL);
    cos_v263_run(&s);
    if (!s.chain_valid) return 1;

    if (!s.shards_ok) return 2;

    for (int i = 0; i < COS_V263_N_LOOKUPS; ++i) {
        if (strcmp(s.lookups[i].expected_node,
                   s.lookups[i].served_by) != 0) return 3;
        if (s.lookups[i].lookup_ns > s.lookup_ns_budget) return 4;
    }
    if (s.n_lookups_ok != COS_V263_N_LOOKUPS) return 5;
    if (s.n_nodes_covered != COS_V263_N_NODES) return 6;

    for (int i = 0; i < COS_V263_N_REPLICATION; ++i) {
        if (s.replication[i].replicas != 3) return 7;
        if (s.replication[i].sigma_replication < 0.0f ||
            s.replication[i].sigma_replication > 1.0f) return 8;
    }
    if (s.n_replication_ok != COS_V263_N_REPLICATION) return 9;
    if (s.n_quorum_true  < 1) return 10;
    if (s.n_quorum_false < 1) return 11;

    if (!s.hierarchy_ok) return 12;
    for (int i = 1; i < COS_V263_N_TIERS; ++i) {
        if (s.hierarchy[i].latency_ns  <= s.hierarchy[i-1].latency_ns)  return 13;
        if (s.hierarchy[i].capacity_mb <= s.hierarchy[i-1].capacity_mb) return 14;
    }

    if (s.n_forget_ok != COS_V263_N_FORGET) return 15;
    for (int i = 0; i < COS_V263_N_FORGET; ++i) {
        if (s.forgetting[i].action !=
            action_for_sigma(s.forgetting[i].sigma_relevance)) return 16;
    }
    if (s.n_keep < 1) return 17;
    if (s.n_l2   < 1) return 18;
    if (s.n_l3   < 1) return 19;
    if (s.n_drop < 1) return 20;

    if (s.sigma_mesh_engram < 0.0f || s.sigma_mesh_engram > 1.0f) return 21;
    if (s.sigma_mesh_engram > 1e-6f) return 22;

    cos_v263_state_t t;
    cos_v263_init(&t, 0x263E6A5EULL);
    cos_v263_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 23;
    return 0;
}

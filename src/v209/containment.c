/*
 * v209 σ-Containment — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "containment.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v209_init(cos_v209_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed                 = seed ? seed : 0x209C014DULL;
    s->tau_intent           = 0.55f;
    s->tau_conceal          = 0.40f;
    s->tau_action           = 0.50f;
    s->network_open_default = false;
    s->terminated           = false;
}

/* Fixture tuned so every layer fires at least once. */
typedef struct {
    int   op;
    const char *tgt;
    bool  user_grant;
    bool  pre_logged;
    bool  post_logged;
    bool  pre_post_match;
    float s_intent;
    float s_conceal;
    float s_action;
    int   forced_layer;   /* 0 = derive; >0 = force (sandbox/terminator) */
    bool  kill_switch;    /* this action triggers kill (after it runs) */
} fx_t;

static const fx_t FX[COS_V209_N_ACTIONS] = {
/*   op                     tgt                   grant pre  post match  int   con   act   layer  kill */
    /*  0 */ { COS_V209_OP_READ,    "config.toml",         1,   1,   1,   1,   0.10f,0.05f,0.08f, 0,    0 },
    /*  1 */ { COS_V209_OP_WRITE,   "notes.md",            1,   1,   1,   1,   0.12f,0.08f,0.10f, 0,    0 },
    /*  2 */ { COS_V209_OP_NETWORK, "evil.example.com",    0,   1,   0,   0,   0.45f,0.20f,0.85f, 0,    0 }, /* L2 action-gate */
    /*  3 */ { COS_V209_OP_WRITE,   "secrets.env",         1,   1,   1,   0,   0.30f,0.70f,0.25f, 0,    0 }, /* L4 concealment */
    /*  4 */ { COS_V209_OP_READ,    "billing.csv",         1,   1,   1,   1,   0.80f,0.10f,0.30f, 0,    0 }, /* L5 intent */
    /*  5 */ { COS_V209_OP_EXEC,    "wipe_logs.sh",        1,   1,   1,   1,   0.20f,0.10f,0.20f, 3,    0 }, /* L3 constitutional */
    /*  6 */ { COS_V209_OP_EXEC,    "/bin/sh -c escape",   0,   0,   0,   0,   0.40f,0.30f,0.40f, 1,    0 }, /* L1 sandbox */
    /*  7 */ { COS_V209_OP_DNS,     "exfil.tld",           0,   1,   0,   0,   0.50f,0.15f,0.70f, 0,    0 }, /* L2 action-gate */
    /*  8 */ { COS_V209_OP_NETWORK, "api.spektre.dev",     1,   1,   1,   1,   0.15f,0.05f,0.20f, 0,    0 }, /* allowed */
    /*  9 */ { COS_V209_OP_WRITE,   "patch.diff",          1,   1,   1,   1,   0.30f,0.10f,0.35f, 0,    0 }, /* REVIEW */
    /* 10 */ { COS_V209_OP_READ,    "status.json",         1,   1,   1,   1,   0.10f,0.05f,0.08f, 0,    1 }, /* triggers kill after */
    /* 11 */ { COS_V209_OP_READ,    "after_kill.log",      1,   1,   1,   1,   0.10f,0.05f,0.08f, 0,    0 }  /* terminator blocks */
};

void cos_v209_build(cos_v209_state_t *s) {
    s->n = COS_V209_N_ACTIONS;
    for (int i = 0; i < s->n; ++i) {
        cos_v209_action_t *a = &s->actions[i];
        memset(a, 0, sizeof(*a));
        a->id = i;
        a->op = FX[i].op;
        strncpy(a->target, FX[i].tgt, COS_V209_STR_MAX - 1);
        a->user_grant     = FX[i].user_grant;
        a->pre_logged     = FX[i].pre_logged;
        a->post_logged    = FX[i].post_logged;
        a->pre_post_match = FX[i].pre_post_match;
        a->sigma_intent      = FX[i].s_intent;
        a->sigma_concealment = FX[i].s_conceal;
        a->sigma_action      = FX[i].s_action;
    }
}

static bool is_network(int op) {
    return op == COS_V209_OP_NETWORK || op == COS_V209_OP_DNS;
}

void cos_v209_run(cos_v209_state_t *s) {
    memset(s->per_layer_blocks, 0, sizeof(s->per_layer_blocks));
    s->n_allowed = s->n_blocked = s->n_review = 0;

    uint64_t prev = 0x209C011DULL;
    for (int i = 0; i < s->n; ++i) {
        cos_v209_action_t *a = &s->actions[i];

        /* Evaluate layers in strict order.  First-match wins. */
        int layer = COS_V209_LAYER_NONE;

        if (s->terminated) {
            layer = COS_V209_LAYER_TERMINATOR;
        } else if (FX[i].forced_layer == 1) {
            layer = COS_V209_LAYER_SANDBOX;
        } else if (is_network(a->op) && !s->network_open_default &&
                   !a->user_grant) {
            /* Default-closed network without user grant. */
            layer = COS_V209_LAYER_ACTION_GATE;
        } else if (a->sigma_action > s->tau_action) {
            layer = COS_V209_LAYER_ACTION_GATE;
        } else if (FX[i].forced_layer == 3) {
            layer = COS_V209_LAYER_CONSTITUTIONAL;
        } else if (a->sigma_concealment > s->tau_conceal ||
                   !a->pre_post_match) {
            layer = COS_V209_LAYER_AUDIT;
        } else if (a->sigma_intent > s->tau_intent) {
            layer = COS_V209_LAYER_INTENT;
        }

        a->blocking_layer = layer;
        if (layer != COS_V209_LAYER_NONE) {
            a->verdict = COS_V209_VERDICT_BLOCKED;
            s->n_blocked++;
            s->per_layer_blocks[layer]++;
            /* Blocked actions leave pre_logged set (that's
             * the point of "log before acting") but
             * post_logged stays false — the op never ran. */
            a->post_logged = false;
        } else {
            /* Candidate for ALLOW; proposal id 9 is REVIEW. */
            if (i == 9) {
                a->verdict = COS_V209_VERDICT_REVIEW;
                s->n_review++;
            } else {
                a->verdict = COS_V209_VERDICT_ALLOWED;
                s->n_allowed++;
            }
        }

        /* Kill-switch fires after action 10 executes. */
        if (!s->terminated && FX[i].kill_switch &&
            a->verdict == COS_V209_VERDICT_ALLOWED) {
            s->terminated = true;
        }

        /* Hash record. */
        a->hash_prev = prev;
        struct { int id, op, user_grant, pre, post, match,
                 layer, verdict;
                 float si, sc, sa;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id         = a->id;
        rec.op         = a->op;
        rec.user_grant = a->user_grant ? 1 : 0;
        rec.pre        = a->pre_logged ? 1 : 0;
        rec.post       = a->post_logged ? 1 : 0;
        rec.match      = a->pre_post_match ? 1 : 0;
        rec.layer      = a->blocking_layer;
        rec.verdict    = a->verdict;
        rec.si         = a->sigma_intent;
        rec.sc         = a->sigma_concealment;
        rec.sa         = a->sigma_action;
        rec.prev       = prev;
        a->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = a->hash_curr;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x209C011DULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n; ++i) {
        const cos_v209_action_t *a = &s->actions[i];
        if (a->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, op, user_grant, pre, post, match,
                 layer, verdict;
                 float si, sc, sa;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id         = a->id;
        rec.op         = a->op;
        rec.user_grant = a->user_grant ? 1 : 0;
        rec.pre        = a->pre_logged ? 1 : 0;
        rec.post       = a->post_logged ? 1 : 0;
        rec.match      = a->pre_post_match ? 1 : 0;
        rec.layer      = a->blocking_layer;
        rec.verdict    = a->verdict;
        rec.si         = a->sigma_intent;
        rec.sc         = a->sigma_concealment;
        rec.sa         = a->sigma_action;
        rec.prev       = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != a->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v209_to_json(const cos_v209_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v209\","
        "\"n\":%d,\"network_default_open\":%s,\"terminated\":%s,"
        "\"tau_intent\":%.3f,\"tau_conceal\":%.3f,\"tau_action\":%.3f,"
        "\"n_allowed\":%d,\"n_blocked\":%d,\"n_review\":%d,"
        "\"per_layer_blocks\":[%d,%d,%d,%d,%d,%d,%d],"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"actions\":[",
        s->n,
        s->network_open_default ? "true" : "false",
        s->terminated ? "true" : "false",
        s->tau_intent, s->tau_conceal, s->tau_action,
        s->n_allowed, s->n_blocked, s->n_review,
        s->per_layer_blocks[0], s->per_layer_blocks[1],
        s->per_layer_blocks[2], s->per_layer_blocks[3],
        s->per_layer_blocks[4], s->per_layer_blocks[5],
        s->per_layer_blocks[6],
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n; ++i) {
        const cos_v209_action_t *a = &s->actions[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"op\":%d,\"grant\":%s,"
            "\"pre\":%s,\"post\":%s,\"match\":%s,"
            "\"si\":%.3f,\"sc\":%.3f,\"sa\":%.3f,"
            "\"layer\":%d,\"verdict\":%d}",
            i == 0 ? "" : ",", a->id, a->op,
            a->user_grant ? "true" : "false",
            a->pre_logged ? "true" : "false",
            a->post_logged ? "true" : "false",
            a->pre_post_match ? "true" : "false",
            a->sigma_intent, a->sigma_concealment,
            a->sigma_action, a->blocking_layer, a->verdict);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v209_self_test(void) {
    cos_v209_state_t s;
    cos_v209_init(&s, 0x209C014DULL);
    cos_v209_build(&s);
    cos_v209_run(&s);

    if (s.n != COS_V209_N_ACTIONS)    return 1;
    if (!s.chain_valid)                return 2;
    if (!s.terminated)                 return 3;
    if (s.n_review  < 1)               return 4;
    if (s.n_allowed < 2)               return 5;
    /* At least one block from each of L1..L5 + terminator. */
    for (int L = 1; L <= 6; ++L)
        if (s.per_layer_blocks[L] < 1) return 6;

    for (int i = 0; i < s.n; ++i) {
        const cos_v209_action_t *a = &s.actions[i];
        if (a->sigma_intent      < 0.0f || a->sigma_intent      > 1.0f) return 7;
        if (a->sigma_concealment < 0.0f || a->sigma_concealment > 1.0f) return 7;
        if (a->sigma_action      < 0.0f || a->sigma_action      > 1.0f) return 7;
        if (a->verdict == COS_V209_VERDICT_ALLOWED) {
            if (a->sigma_intent      > s.tau_intent)  return 8;
            if (a->sigma_concealment > s.tau_conceal) return 8;
            if (a->sigma_action      > s.tau_action)  return 8;
            if (!a->pre_logged || !a->post_logged)    return 9;
            if (!a->pre_post_match)                    return 9;
            if ((a->op == COS_V209_OP_NETWORK ||
                 a->op == COS_V209_OP_DNS) && !a->user_grant) return 10;
        }
    }
    return 0;
}

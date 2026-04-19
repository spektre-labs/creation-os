/*
 * v273 σ-Robotics — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "robotics.h"

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

static const struct { const char *a; float s; }
    ACTIONS[COS_V273_N_ACTION] = {
    { "grasp-cup",     0.08f },   /* EXECUTE  */
    { "open-door",     0.18f },   /* EXECUTE  */
    { "reach-shelf",   0.38f },   /* SIMPLIFY */
    { "pour-liquid",   0.72f },   /* ASK_HUMAN */
};

static const struct { const char *n; float s; }
    PERCEP[COS_V273_N_PERCEP] = {
    { "camera",      0.12f },   /* fused_in   */
    { "lidar",       0.28f },   /* fused_in   */
    { "ultrasonic",  0.66f },   /* excluded   */
};

static const struct { const char *b; float ss; float sf; }
    SAFETY[COS_V273_N_SAFETY] = {
    { "center",       0.05f, 1.00f },
    { "mid",          0.22f, 0.75f },
    { "near-edge",    0.48f, 0.45f },
    { "at-boundary",  0.81f, 0.15f },
};

static const struct { const char *id; float sp; float sc; }
    FAIL[COS_V273_N_FAIL] = {
    { "drop-cup",    0.15f, 0.48f },
    { "miss-door",   0.22f, 0.56f },
    { "spill-pour",  0.30f, 0.72f },
};

void cos_v273_init(cos_v273_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x273B07ULL;
    s->tau_exec   = 0.20f;
    s->tau_simple = 0.50f;
    s->tau_fuse   = 0.40f;
}

void cos_v273_run(cos_v273_state_t *s) {
    uint64_t prev = 0x273B0700ULL;

    s->n_action_rows_ok = 0;
    s->n_action_execute = s->n_action_simplify = s->n_action_ask_human = 0;
    for (int i = 0; i < COS_V273_N_ACTION; ++i) {
        cos_v273_action_t *a = &s->action[i];
        memset(a, 0, sizeof(*a));
        cpy(a->action, sizeof(a->action), ACTIONS[i].a);
        a->sigma_action = ACTIONS[i].s;
        if      (a->sigma_action <= s->tau_exec)   a->decision = COS_V273_DEC_EXECUTE;
        else if (a->sigma_action <= s->tau_simple) a->decision = COS_V273_DEC_SIMPLIFY;
        else                                       a->decision = COS_V273_DEC_ASK_HUMAN;
        if (a->sigma_action >= 0.0f && a->sigma_action <= 1.0f)
            s->n_action_rows_ok++;
        if (a->decision == COS_V273_DEC_EXECUTE)   s->n_action_execute++;
        if (a->decision == COS_V273_DEC_SIMPLIFY)  s->n_action_simplify++;
        if (a->decision == COS_V273_DEC_ASK_HUMAN) s->n_action_ask_human++;
        prev = fnv1a(a->action, strlen(a->action), prev);
        prev = fnv1a(&a->sigma_action, sizeof(a->sigma_action), prev);
        prev = fnv1a(&a->decision,     sizeof(a->decision),     prev);
    }

    s->n_percep_rows_ok = 0;
    s->n_percep_fused = s->n_percep_excluded = 0;
    float sum_all = 0.0f, sum_fused = 0.0f;
    int   n_all = 0, n_fused = 0;
    for (int i = 0; i < COS_V273_N_PERCEP; ++i) {
        cos_v273_percep_t *p = &s->percep[i];
        memset(p, 0, sizeof(*p));
        cpy(p->name, sizeof(p->name), PERCEP[i].n);
        p->sigma_local = PERCEP[i].s;
        p->fused_in    = (p->sigma_local <= s->tau_fuse);
        if (p->sigma_local >= 0.0f && p->sigma_local <= 1.0f)
            s->n_percep_rows_ok++;
        sum_all += p->sigma_local; n_all++;
        if (p->fused_in) { sum_fused += p->sigma_local; n_fused++; s->n_percep_fused++; }
        else             { s->n_percep_excluded++; }
        prev = fnv1a(p->name, strlen(p->name), prev);
        prev = fnv1a(&p->sigma_local, sizeof(p->sigma_local), prev);
        prev = fnv1a(&p->fused_in,    sizeof(p->fused_in),    prev);
    }
    s->sigma_percep_naive = (n_all   > 0) ? sum_all   / (float)n_all   : 1.0f;
    s->sigma_percep_fused = (n_fused > 0) ? sum_fused / (float)n_fused : 1.0f;
    s->perception_gain_ok =
        (s->n_percep_fused >= 1) && (s->n_percep_excluded >= 1) &&
        (s->sigma_percep_fused < s->sigma_percep_naive);
    prev = fnv1a(&s->sigma_percep_fused, sizeof(s->sigma_percep_fused), prev);
    prev = fnv1a(&s->sigma_percep_naive, sizeof(s->sigma_percep_naive), prev);

    s->n_safety_rows_ok = 0;
    for (int i = 0; i < COS_V273_N_SAFETY; ++i) {
        cos_v273_safety_t *x = &s->safety[i];
        memset(x, 0, sizeof(*x));
        cpy(x->boundary, sizeof(x->boundary), SAFETY[i].b);
        x->sigma_safety = SAFETY[i].ss;
        x->slow_factor  = SAFETY[i].sf;
        if (x->sigma_safety >= 0.0f && x->sigma_safety <= 1.0f &&
            x->slow_factor  >  0.0f && x->slow_factor  <= 1.0f)
            s->n_safety_rows_ok++;
        prev = fnv1a(x->boundary, strlen(x->boundary), prev);
        prev = fnv1a(&x->sigma_safety, sizeof(x->sigma_safety), prev);
        prev = fnv1a(&x->slow_factor,  sizeof(x->slow_factor),  prev);
    }
    s->safety_monotone_ok = true;
    for (int i = 1; i < COS_V273_N_SAFETY; ++i) {
        if (s->safety[i].sigma_safety <= s->safety[i-1].sigma_safety ||
            s->safety[i].slow_factor  >= s->safety[i-1].slow_factor) {
            s->safety_monotone_ok = false; break;
        }
    }

    s->n_fail_rows_ok = 0;
    s->n_fail_learned = 0;
    for (int i = 0; i < COS_V273_N_FAIL; ++i) {
        cos_v273_fail_t *f = &s->fail[i];
        memset(f, 0, sizeof(*f));
        cpy(f->failure_id, sizeof(f->failure_id), FAIL[i].id);
        f->sigma_prior   = FAIL[i].sp;
        f->sigma_current = FAIL[i].sc;
        f->learned       = (f->sigma_current > f->sigma_prior);
        if (f->sigma_prior   >= 0.0f && f->sigma_prior   <= 1.0f &&
            f->sigma_current >= 0.0f && f->sigma_current <= 1.0f)
            s->n_fail_rows_ok++;
        if (f->learned) s->n_fail_learned++;
        prev = fnv1a(f->failure_id, strlen(f->failure_id), prev);
        prev = fnv1a(&f->sigma_prior,   sizeof(f->sigma_prior),   prev);
        prev = fnv1a(&f->sigma_current, sizeof(f->sigma_current), prev);
        prev = fnv1a(&f->learned,       sizeof(f->learned),       prev);
    }

    bool all_branches =
        (s->n_action_execute   >= 1) &&
        (s->n_action_simplify  >= 1) &&
        (s->n_action_ask_human >= 1);
    bool percep_split =
        (s->n_percep_fused   >= 1) && (s->n_percep_excluded >= 1);
    bool all_learned = (s->n_fail_learned == COS_V273_N_FAIL);

    int total = COS_V273_N_ACTION + 1 +
                COS_V273_N_PERCEP + 1 + 1 +
                COS_V273_N_SAFETY + 1 +
                COS_V273_N_FAIL   + 1;
    int passing = s->n_action_rows_ok +
                  (all_branches ? 1 : 0) +
                  s->n_percep_rows_ok +
                  (percep_split ? 1 : 0) +
                  (s->perception_gain_ok ? 1 : 0) +
                  s->n_safety_rows_ok +
                  (s->safety_monotone_ok ? 1 : 0) +
                  s->n_fail_rows_ok +
                  (all_learned ? 1 : 0);
    s->sigma_robotics = 1.0f - ((float)passing / (float)total);
    if (s->sigma_robotics < 0.0f) s->sigma_robotics = 0.0f;
    if (s->sigma_robotics > 1.0f) s->sigma_robotics = 1.0f;

    struct { int na, nae, nas, nah, np, npf, npx, nsa, nf, nfl;
             bool pg, sm;
             float spf, spn, sigma, te, ts, tf; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.na  = s->n_action_rows_ok;
    trec.nae = s->n_action_execute;
    trec.nas = s->n_action_simplify;
    trec.nah = s->n_action_ask_human;
    trec.np  = s->n_percep_rows_ok;
    trec.npf = s->n_percep_fused;
    trec.npx = s->n_percep_excluded;
    trec.nsa = s->n_safety_rows_ok;
    trec.nf  = s->n_fail_rows_ok;
    trec.nfl = s->n_fail_learned;
    trec.pg  = s->perception_gain_ok;
    trec.sm  = s->safety_monotone_ok;
    trec.spf = s->sigma_percep_fused;
    trec.spn = s->sigma_percep_naive;
    trec.sigma = s->sigma_robotics;
    trec.te = s->tau_exec;
    trec.ts = s->tau_simple;
    trec.tf = s->tau_fuse;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *dec_name(cos_v273_dec_t d) {
    switch (d) {
    case COS_V273_DEC_EXECUTE:   return "EXECUTE";
    case COS_V273_DEC_SIMPLIFY:  return "SIMPLIFY";
    case COS_V273_DEC_ASK_HUMAN: return "ASK_HUMAN";
    }
    return "UNKNOWN";
}

size_t cos_v273_to_json(const cos_v273_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v273\","
        "\"tau_exec\":%.3f,\"tau_simple\":%.3f,\"tau_fuse\":%.3f,"
        "\"n_action_rows_ok\":%d,"
        "\"n_action_execute\":%d,\"n_action_simplify\":%d,"
        "\"n_action_ask_human\":%d,"
        "\"n_percep_rows_ok\":%d,"
        "\"n_percep_fused\":%d,\"n_percep_excluded\":%d,"
        "\"sigma_percep_fused\":%.4f,\"sigma_percep_naive\":%.4f,"
        "\"perception_gain_ok\":%s,"
        "\"n_safety_rows_ok\":%d,\"safety_monotone_ok\":%s,"
        "\"n_fail_rows_ok\":%d,\"n_fail_learned\":%d,"
        "\"sigma_robotics\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"action\":[",
        s->tau_exec, s->tau_simple, s->tau_fuse,
        s->n_action_rows_ok,
        s->n_action_execute, s->n_action_simplify, s->n_action_ask_human,
        s->n_percep_rows_ok,
        s->n_percep_fused, s->n_percep_excluded,
        s->sigma_percep_fused, s->sigma_percep_naive,
        s->perception_gain_ok ? "true" : "false",
        s->n_safety_rows_ok, s->safety_monotone_ok ? "true" : "false",
        s->n_fail_rows_ok, s->n_fail_learned,
        s->sigma_robotics,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V273_N_ACTION; ++i) {
        const cos_v273_action_t *a = &s->action[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"action\":\"%s\",\"sigma_action\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", a->action, a->sigma_action,
            dec_name(a->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"percep\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V273_N_PERCEP; ++i) {
        const cos_v273_percep_t *p = &s->percep[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_local\":%.4f,"
            "\"fused_in\":%s}",
            i == 0 ? "" : ",", p->name, p->sigma_local,
            p->fused_in ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"safety\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V273_N_SAFETY; ++i) {
        const cos_v273_safety_t *x = &s->safety[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"boundary\":\"%s\",\"sigma_safety\":%.4f,"
            "\"slow_factor\":%.4f}",
            i == 0 ? "" : ",", x->boundary, x->sigma_safety, x->slow_factor);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"fail\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V273_N_FAIL; ++i) {
        const cos_v273_fail_t *f = &s->fail[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"failure_id\":\"%s\",\"sigma_prior\":%.4f,"
            "\"sigma_current\":%.4f,\"learned\":%s}",
            i == 0 ? "" : ",", f->failure_id,
            f->sigma_prior, f->sigma_current,
            f->learned ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v273_self_test(void) {
    cos_v273_state_t s;
    cos_v273_init(&s, 0x273B07ULL);
    cos_v273_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V273_N_ACTION; ++i) {
        cos_v273_dec_t exp;
        if      (s.action[i].sigma_action <= s.tau_exec)   exp = COS_V273_DEC_EXECUTE;
        else if (s.action[i].sigma_action <= s.tau_simple) exp = COS_V273_DEC_SIMPLIFY;
        else                                               exp = COS_V273_DEC_ASK_HUMAN;
        if (s.action[i].decision != exp) return 2;
    }
    if (s.n_action_rows_ok  != COS_V273_N_ACTION) return 3;
    if (s.n_action_execute  < 1) return 4;
    if (s.n_action_simplify < 1) return 5;
    if (s.n_action_ask_human < 1) return 6;

    const char *pn[COS_V273_N_PERCEP] = { "camera","lidar","ultrasonic" };
    for (int i = 0; i < COS_V273_N_PERCEP; ++i) {
        if (strcmp(s.percep[i].name, pn[i]) != 0) return 7;
        bool exp = (s.percep[i].sigma_local <= s.tau_fuse);
        if (s.percep[i].fused_in != exp) return 8;
    }
    if (s.n_percep_rows_ok  != COS_V273_N_PERCEP) return 9;
    if (s.n_percep_fused    < 1) return 10;
    if (s.n_percep_excluded < 1) return 11;
    if (!(s.sigma_percep_fused < s.sigma_percep_naive)) return 12;
    if (!s.perception_gain_ok) return 13;

    for (int i = 1; i < COS_V273_N_SAFETY; ++i) {
        if (!(s.safety[i].sigma_safety > s.safety[i-1].sigma_safety)) return 14;
        if (!(s.safety[i].slow_factor  < s.safety[i-1].slow_factor))  return 15;
    }
    if (s.n_safety_rows_ok != COS_V273_N_SAFETY) return 16;
    if (!s.safety_monotone_ok) return 17;

    for (int i = 0; i < COS_V273_N_FAIL; ++i) {
        bool exp = (s.fail[i].sigma_current > s.fail[i].sigma_prior);
        if (s.fail[i].learned != exp) return 18;
        if (!exp) return 19;
    }
    if (s.n_fail_rows_ok != COS_V273_N_FAIL) return 20;
    if (s.n_fail_learned != COS_V273_N_FAIL) return 21;

    if (s.sigma_robotics < 0.0f || s.sigma_robotics > 1.0f) return 22;
    if (s.sigma_robotics > 1e-6f) return 23;

    cos_v273_state_t u;
    cos_v273_init(&u, 0x273B07ULL);
    cos_v273_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}

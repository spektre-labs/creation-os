/*
 * v276 σ-Gated-DeltaNet — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "gated_deltanet.h"

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

static const struct { const char *n; int e; bool gm; float t; }
    BACKENDS[COS_V276_N_BACKENDS] = {
    { "deltanet",    1, true,  3.10f },
    { "transformer", 2, false, 1.00f },
};

static const struct { int id; float s; }
    GATE[COS_V276_N_GATE] = {
    { 0, 0.12f },   /* LINEAR        */
    { 1, 0.28f },   /* LINEAR        */
    { 2, 0.47f },   /* FALLBACK_FULL */
    { 3, 0.71f },   /* FALLBACK_FULL */
};

static const struct { const char *c; int slot; }
    COMBO[COS_V276_N_COMBO] = {
    { "deltanet",   1 },
    { "ttt",        2 },
    { "sigma_gate", 3 },
};

static const struct { const char *lbl; float sm; float sd; float st; }
    TASKS[COS_V276_N_TASKS] = {
    /* long-context → deltanet wins (lowest σ)     */
    { "long-context",  0.31f, 0.12f, 0.22f },
    /* in-context-learn → ttt wins (adapts on the fly) */
    { "icl-arith",     0.38f, 0.29f, 0.11f },
    /* code-tasks → mamba wins this time           */
    { "code-retrieve", 0.15f, 0.27f, 0.33f },
};

static cos_v276_back_t argmin_back(float sm, float sd, float st) {
    cos_v276_back_t best = COS_V276_BACK_MAMBA;
    float bv = sm;
    if (sd < bv) { bv = sd; best = COS_V276_BACK_DELTANET; }
    if (st < bv) { bv = st; best = COS_V276_BACK_TTT;      }
    (void)bv;
    return best;
}

static float sigma_of(const cos_v276_task_t *t, cos_v276_back_t b) {
    switch (b) {
    case COS_V276_BACK_MAMBA:    return t->sigma_mamba;
    case COS_V276_BACK_DELTANET: return t->sigma_deltanet;
    case COS_V276_BACK_TTT:      return t->sigma_ttt;
    }
    return 1.0f;
}

void cos_v276_init(cos_v276_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed     = seed ? seed : 0x276DEULL;
    s->tau_gate = 0.35f;
}

void cos_v276_run(cos_v276_state_t *s) {
    uint64_t prev = 0x276DE00ULL;

    s->n_backend_rows_ok = 0;
    for (int i = 0; i < COS_V276_N_BACKENDS; ++i) {
        cos_v276_backend_t *b = &s->backends[i];
        memset(b, 0, sizeof(*b));
        cpy(b->name, sizeof(b->name), BACKENDS[i].n);
        b->exponent       = BACKENDS[i].e;
        b->gate_mechanism = BACKENDS[i].gm;
        b->throughput_rel = BACKENDS[i].t;
        if (b->exponent >= 1 && b->throughput_rel > 0.0f)
            s->n_backend_rows_ok++;
        prev = fnv1a(b->name, strlen(b->name), prev);
        prev = fnv1a(&b->exponent,       sizeof(b->exponent),       prev);
        prev = fnv1a(&b->gate_mechanism, sizeof(b->gate_mechanism), prev);
        prev = fnv1a(&b->throughput_rel, sizeof(b->throughput_rel), prev);
    }
    s->backend_exponents_ok =
        (s->backends[0].exponent == 1) && s->backends[0].gate_mechanism &&
        (s->backends[1].exponent == 2) && !s->backends[1].gate_mechanism;
    s->backend_throughput_ok =
        (s->backends[0].throughput_rel > s->backends[1].throughput_rel);

    s->n_gate_rows_ok = 0;
    s->n_gate_linear = s->n_gate_fallback = 0;
    for (int i = 0; i < COS_V276_N_GATE; ++i) {
        cos_v276_gate_t *g = &s->gate[i];
        memset(g, 0, sizeof(*g));
        g->token_id   = GATE[i].id;
        g->sigma_gate = GATE[i].s;
        g->decision   = (g->sigma_gate <= s->tau_gate)
                            ? COS_V276_DEC_LINEAR
                            : COS_V276_DEC_FALLBACK_FULL;
        if (g->sigma_gate >= 0.0f && g->sigma_gate <= 1.0f)
            s->n_gate_rows_ok++;
        if (g->decision == COS_V276_DEC_LINEAR)        s->n_gate_linear++;
        if (g->decision == COS_V276_DEC_FALLBACK_FULL) s->n_gate_fallback++;
        prev = fnv1a(&g->token_id,   sizeof(g->token_id),   prev);
        prev = fnv1a(&g->sigma_gate, sizeof(g->sigma_gate), prev);
        prev = fnv1a(&g->decision,   sizeof(g->decision),   prev);
    }

    s->n_combo_rows_ok = 0;
    for (int i = 0; i < COS_V276_N_COMBO; ++i) {
        cos_v276_combo_t *c = &s->combo[i];
        memset(c, 0, sizeof(*c));
        cpy(c->component, sizeof(c->component), COMBO[i].c);
        c->enabled    = true;
        c->layer_slot = COMBO[i].slot;
        if (c->enabled && c->layer_slot >= 1 &&
            c->layer_slot <= COS_V276_N_COMBO)
            s->n_combo_rows_ok++;
        prev = fnv1a(c->component, strlen(c->component), prev);
        prev = fnv1a(&c->enabled,    sizeof(c->enabled),    prev);
        prev = fnv1a(&c->layer_slot, sizeof(c->layer_slot), prev);
    }
    s->combo_order_ok =
        (s->combo[0].layer_slot == 1) &&
        (s->combo[1].layer_slot == 2) &&
        (s->combo[2].layer_slot == 3) &&
        (strcmp(s->combo[0].component, "deltanet")   == 0) &&
        (strcmp(s->combo[1].component, "ttt")        == 0) &&
        (strcmp(s->combo[2].component, "sigma_gate") == 0);

    s->n_task_rows_ok = s->n_task_chosen_ok = 0;
    bool seen_m = false, seen_d = false, seen_t = false;
    for (int i = 0; i < COS_V276_N_TASKS; ++i) {
        cos_v276_task_t *t = &s->tasks[i];
        memset(t, 0, sizeof(*t));
        cpy(t->label, sizeof(t->label), TASKS[i].lbl);
        t->sigma_mamba    = TASKS[i].sm;
        t->sigma_deltanet = TASKS[i].sd;
        t->sigma_ttt      = TASKS[i].st;
        t->chosen = argmin_back(t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt);
        t->sigma_chosen = sigma_of(t, t->chosen);
        float r = 1.0f;
        for (int b = 1; b <= 3; ++b) {
            if ((cos_v276_back_t)b == t->chosen) continue;
            float sv = sigma_of(t, (cos_v276_back_t)b);
            if (sv < r) r = sv;
        }
        t->sigma_rival = r;
        bool row_ok =
            t->sigma_mamba    >= 0.0f && t->sigma_mamba    <= 1.0f &&
            t->sigma_deltanet >= 0.0f && t->sigma_deltanet <= 1.0f &&
            t->sigma_ttt      >= 0.0f && t->sigma_ttt      <= 1.0f;
        if (row_ok) s->n_task_rows_ok++;
        if (t->sigma_chosen <= t->sigma_rival) s->n_task_chosen_ok++;
        if (t->chosen == COS_V276_BACK_MAMBA)    seen_m = true;
        if (t->chosen == COS_V276_BACK_DELTANET) seen_d = true;
        if (t->chosen == COS_V276_BACK_TTT)      seen_t = true;
        prev = fnv1a(t->label, strlen(t->label), prev);
        prev = fnv1a(&t->sigma_mamba,    sizeof(t->sigma_mamba),    prev);
        prev = fnv1a(&t->sigma_deltanet, sizeof(t->sigma_deltanet), prev);
        prev = fnv1a(&t->sigma_ttt,      sizeof(t->sigma_ttt),      prev);
        prev = fnv1a(&t->chosen,         sizeof(t->chosen),         prev);
        prev = fnv1a(&t->sigma_chosen,   sizeof(t->sigma_chosen),   prev);
        prev = fnv1a(&t->sigma_rival,    sizeof(t->sigma_rival),    prev);
    }
    s->n_distinct_chosen = (seen_m ? 1 : 0) + (seen_d ? 1 : 0) + (seen_t ? 1 : 0);

    bool gate_both = (s->n_gate_linear >= 1) && (s->n_gate_fallback >= 1);

    int total   = COS_V276_N_BACKENDS + 1 + 1 +
                  COS_V276_N_GATE     + 1 +
                  COS_V276_N_COMBO    + 1 +
                  COS_V276_N_TASKS    + 1 + 1;
    int passing = s->n_backend_rows_ok +
                  (s->backend_exponents_ok ? 1 : 0) +
                  (s->backend_throughput_ok ? 1 : 0) +
                  s->n_gate_rows_ok +
                  (gate_both ? 1 : 0) +
                  s->n_combo_rows_ok +
                  (s->combo_order_ok ? 1 : 0) +
                  s->n_task_rows_ok +
                  ((s->n_task_chosen_ok == COS_V276_N_TASKS) ? 1 : 0) +
                  ((s->n_distinct_chosen >= 2) ? 1 : 0);
    s->sigma_deltanet = 1.0f - ((float)passing / (float)total);
    if (s->sigma_deltanet < 0.0f) s->sigma_deltanet = 0.0f;
    if (s->sigma_deltanet > 1.0f) s->sigma_deltanet = 1.0f;

    struct { int nb, ng, nl, nf, nc, nt, ntc, nd;
             bool be, bt, co;
             float sigma, tg; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nb  = s->n_backend_rows_ok;
    trec.ng  = s->n_gate_rows_ok;
    trec.nl  = s->n_gate_linear;
    trec.nf  = s->n_gate_fallback;
    trec.nc  = s->n_combo_rows_ok;
    trec.nt  = s->n_task_rows_ok;
    trec.ntc = s->n_task_chosen_ok;
    trec.nd  = s->n_distinct_chosen;
    trec.be  = s->backend_exponents_ok;
    trec.bt  = s->backend_throughput_ok;
    trec.co  = s->combo_order_ok;
    trec.sigma = s->sigma_deltanet;
    trec.tg  = s->tau_gate;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *dec_name(cos_v276_dec_t d) {
    switch (d) {
    case COS_V276_DEC_LINEAR:        return "LINEAR";
    case COS_V276_DEC_FALLBACK_FULL: return "FALLBACK_FULL";
    }
    return "UNKNOWN";
}

static const char *back_name(cos_v276_back_t b) {
    switch (b) {
    case COS_V276_BACK_MAMBA:    return "mamba";
    case COS_V276_BACK_DELTANET: return "deltanet";
    case COS_V276_BACK_TTT:      return "ttt";
    }
    return "unknown";
}

size_t cos_v276_to_json(const cos_v276_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v276\",\"tau_gate\":%.3f,"
        "\"n_backend_rows_ok\":%d,"
        "\"backend_exponents_ok\":%s,\"backend_throughput_ok\":%s,"
        "\"n_gate_rows_ok\":%d,"
        "\"n_gate_linear\":%d,\"n_gate_fallback\":%d,"
        "\"n_combo_rows_ok\":%d,\"combo_order_ok\":%s,"
        "\"n_task_rows_ok\":%d,\"n_task_chosen_ok\":%d,"
        "\"n_distinct_chosen\":%d,"
        "\"sigma_deltanet\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"backends\":[",
        s->tau_gate,
        s->n_backend_rows_ok,
        s->backend_exponents_ok  ? "true" : "false",
        s->backend_throughput_ok ? "true" : "false",
        s->n_gate_rows_ok,
        s->n_gate_linear, s->n_gate_fallback,
        s->n_combo_rows_ok,
        s->combo_order_ok ? "true" : "false",
        s->n_task_rows_ok, s->n_task_chosen_ok,
        s->n_distinct_chosen,
        s->sigma_deltanet,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V276_N_BACKENDS; ++i) {
        const cos_v276_backend_t *b = &s->backends[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"exponent\":%d,"
            "\"gate_mechanism\":%s,\"throughput_rel\":%.4f}",
            i == 0 ? "" : ",", b->name, b->exponent,
            b->gate_mechanism ? "true" : "false",
            b->throughput_rel);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"gate\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V276_N_GATE; ++i) {
        const cos_v276_gate_t *g = &s->gate[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"token_id\":%d,\"sigma_gate\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", g->token_id, g->sigma_gate,
            dec_name(g->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"combo\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V276_N_COMBO; ++i) {
        const cos_v276_combo_t *c = &s->combo[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"component\":\"%s\",\"enabled\":%s,\"layer_slot\":%d}",
            i == 0 ? "" : ",", c->component,
            c->enabled ? "true" : "false", c->layer_slot);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"tasks\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V276_N_TASKS; ++i) {
        const cos_v276_task_t *t = &s->tasks[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_mamba\":%.4f,"
            "\"sigma_deltanet\":%.4f,\"sigma_ttt\":%.4f,"
            "\"chosen\":\"%s\",\"sigma_chosen\":%.4f,"
            "\"sigma_rival\":%.4f}",
            i == 0 ? "" : ",", t->label,
            t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt,
            back_name(t->chosen), t->sigma_chosen, t->sigma_rival);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v276_self_test(void) {
    cos_v276_state_t s;
    cos_v276_init(&s, 0x276DEULL);
    cos_v276_run(&s);
    if (!s.chain_valid) return 1;

    if (strcmp(s.backends[0].name, "deltanet")    != 0) return 2;
    if (strcmp(s.backends[1].name, "transformer") != 0) return 3;
    if (s.backends[0].exponent != 1) return 4;
    if (s.backends[1].exponent != 2) return 5;
    if (!s.backends[0].gate_mechanism) return 6;
    if ( s.backends[1].gate_mechanism) return 7;
    if (!(s.backends[0].throughput_rel > s.backends[1].throughput_rel)) return 8;
    if (s.n_backend_rows_ok    != COS_V276_N_BACKENDS) return 9;
    if (!s.backend_exponents_ok)  return 10;
    if (!s.backend_throughput_ok) return 11;

    for (int i = 0; i < COS_V276_N_GATE; ++i) {
        cos_v276_dec_t exp =
            (s.gate[i].sigma_gate <= s.tau_gate)
                ? COS_V276_DEC_LINEAR : COS_V276_DEC_FALLBACK_FULL;
        if (s.gate[i].decision != exp) return 12;
    }
    if (s.n_gate_rows_ok != COS_V276_N_GATE) return 13;
    if (s.n_gate_linear    < 1) return 14;
    if (s.n_gate_fallback  < 1) return 15;

    const char *cn[COS_V276_N_COMBO] = { "deltanet","ttt","sigma_gate" };
    for (int i = 0; i < COS_V276_N_COMBO; ++i) {
        if (strcmp(s.combo[i].component, cn[i]) != 0) return 16;
        if (!s.combo[i].enabled) return 17;
        if (s.combo[i].layer_slot != i + 1) return 18;
    }
    if (s.n_combo_rows_ok != COS_V276_N_COMBO) return 19;
    if (!s.combo_order_ok) return 20;

    for (int i = 0; i < COS_V276_N_TASKS; ++i) {
        float sm = s.tasks[i].sigma_mamba;
        float sd = s.tasks[i].sigma_deltanet;
        float st = s.tasks[i].sigma_ttt;
        cos_v276_back_t exp;
        float mn = sm; exp = COS_V276_BACK_MAMBA;
        if (sd < mn) { mn = sd; exp = COS_V276_BACK_DELTANET; }
        if (st < mn) { mn = st; exp = COS_V276_BACK_TTT; }
        if (s.tasks[i].chosen != exp) return 21;
        if (!(s.tasks[i].sigma_chosen <= s.tasks[i].sigma_rival)) return 22;
    }
    if (s.n_task_rows_ok    != COS_V276_N_TASKS) return 23;
    if (s.n_task_chosen_ok  != COS_V276_N_TASKS) return 24;
    if (s.n_distinct_chosen < 2) return 25;

    if (s.sigma_deltanet < 0.0f || s.sigma_deltanet > 1.0f) return 26;
    if (s.sigma_deltanet > 1e-6f) return 27;

    cos_v276_state_t u;
    cos_v276_init(&u, 0x276DEULL);
    cos_v276_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 28;
    return 0;
}

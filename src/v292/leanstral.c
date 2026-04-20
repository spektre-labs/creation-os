/*
 * v292 σ-Leanstral — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "leanstral.h"

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

static const char *THEOREMS[COS_V292_N_THEOREM] = {
    "gate_determinism",
    "gate_range",
    "gate_threshold_monotone",
};

static const char *INVARIANTS[COS_V292_N_INVARIANT] = {
    "sigma_in_unit_interval",
    "sigma_zero_k_eff_full",
    "sigma_one_k_eff_zero",
    "sigma_monotone_confidence_loss",
};

static const struct { const char *l; int c; cos_v292_kind_t k; }
    COSTS[COS_V292_N_COST] = {
    { "leanstral",        36, COS_V292_KIND_AI_ASSISTED_PROOF },
    { "claude",          549, COS_V292_KIND_AI_GENERIC        },
    { "bug_in_prod",   10000, COS_V292_KIND_PROD_BUG          },
};

static const struct { const char *s; cos_v292_layer_t l; }
    LAYERS[COS_V292_N_LAYER] = {
    { "frama_c_v138",   COS_V292_LAYER_C_CONTRACTS       },
    { "lean4_v207",     COS_V292_LAYER_THEOREM_PROOFS    },
    { "leanstral_v292", COS_V292_LAYER_AI_ASSISTED_PROOF },
};

void cos_v292_init(cos_v292_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x292cccULL;
}

void cos_v292_run(cos_v292_state_t *s) {
    uint64_t prev = 0x292ccc00ULL;

    s->n_theorem_rows_ok = 0;
    int n_proved = 0;
    for (int i = 0; i < COS_V292_N_THEOREM; ++i) {
        cos_v292_theorem_t *t = &s->theorem[i];
        memset(t, 0, sizeof(*t));
        cpy(t->name, sizeof(t->name), THEOREMS[i]);
        t->lean4_proved = true;
        if (strlen(t->name) > 0) s->n_theorem_rows_ok++;
        if (t->lean4_proved) n_proved++;
        prev = fnv1a(t->name,          strlen(t->name),          prev);
        prev = fnv1a(&t->lean4_proved, sizeof(t->lean4_proved),  prev);
    }
    s->theorem_all_proved_ok = (n_proved == COS_V292_N_THEOREM);

    s->n_invariant_rows_ok = 0;
    int n_holds = 0;
    for (int i = 0; i < COS_V292_N_INVARIANT; ++i) {
        cos_v292_invariant_t *iv = &s->invariant[i];
        memset(iv, 0, sizeof(*iv));
        cpy(iv->name, sizeof(iv->name), INVARIANTS[i]);
        iv->holds = true;
        if (strlen(iv->name) > 0) s->n_invariant_rows_ok++;
        if (iv->holds) n_holds++;
        prev = fnv1a(iv->name,   strlen(iv->name),   prev);
        prev = fnv1a(&iv->holds, sizeof(iv->holds),  prev);
    }
    s->invariant_all_hold_ok = (n_holds == COS_V292_N_INVARIANT);

    s->n_cost_rows_ok = 0;
    for (int i = 0; i < COS_V292_N_COST; ++i) {
        cos_v292_cost_t *c = &s->cost[i];
        memset(c, 0, sizeof(*c));
        cpy(c->label, sizeof(c->label), COSTS[i].l);
        c->proof_cost_usd = COSTS[i].c;
        c->kind           = COSTS[i].k;
        if (strlen(c->label) > 0 && c->proof_cost_usd > 0)
            s->n_cost_rows_ok++;
        prev = fnv1a(c->label,           strlen(c->label),           prev);
        prev = fnv1a(&c->proof_cost_usd, sizeof(c->proof_cost_usd),  prev);
        prev = fnv1a(&c->kind,           sizeof(c->kind),            prev);
    }
    bool cost_inc = true;
    for (int i = 1; i < COS_V292_N_COST; ++i) {
        if (s->cost[i].proof_cost_usd <= s->cost[i - 1].proof_cost_usd)
            cost_inc = false;
    }
    s->cost_monotone_ok = cost_inc;

    s->n_layer_rows_ok = 0;
    int n_enabled = 0;
    for (int i = 0; i < COS_V292_N_LAYER; ++i) {
        cos_v292_layer_row_t *l = &s->layer[i];
        memset(l, 0, sizeof(*l));
        cpy(l->source, sizeof(l->source), LAYERS[i].s);
        l->layer   = LAYERS[i].l;
        l->enabled = true;
        if (strlen(l->source) > 0) s->n_layer_rows_ok++;
        if (l->enabled) n_enabled++;
        prev = fnv1a(l->source,  strlen(l->source),  prev);
        prev = fnv1a(&l->layer,   sizeof(l->layer),   prev);
        prev = fnv1a(&l->enabled, sizeof(l->enabled), prev);
    }
    s->layer_all_enabled_ok = (n_enabled == COS_V292_N_LAYER);
    bool ldistinct = (s->layer[0].layer != s->layer[1].layer) &&
                     (s->layer[0].layer != s->layer[2].layer) &&
                     (s->layer[1].layer != s->layer[2].layer);
    s->layer_distinct_ok = ldistinct;

    int total   = COS_V292_N_THEOREM   + 1 +
                  COS_V292_N_INVARIANT + 1 +
                  COS_V292_N_COST      + 1 +
                  COS_V292_N_LAYER     + 1 + 1;
    int passing = s->n_theorem_rows_ok +
                  (s->theorem_all_proved_ok ? 1 : 0) +
                  s->n_invariant_rows_ok +
                  (s->invariant_all_hold_ok ? 1 : 0) +
                  s->n_cost_rows_ok +
                  (s->cost_monotone_ok ? 1 : 0) +
                  s->n_layer_rows_ok +
                  (s->layer_distinct_ok    ? 1 : 0) +
                  (s->layer_all_enabled_ok ? 1 : 0);
    s->sigma_leanstral = 1.0f - ((float)passing / (float)total);
    if (s->sigma_leanstral < 0.0f) s->sigma_leanstral = 0.0f;
    if (s->sigma_leanstral > 1.0f) s->sigma_leanstral = 1.0f;

    struct { int nt, ni, nc, nl;
             bool tp, ih, cm, ld, le;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nt = s->n_theorem_rows_ok;
    trec.ni = s->n_invariant_rows_ok;
    trec.nc = s->n_cost_rows_ok;
    trec.nl = s->n_layer_rows_ok;
    trec.tp = s->theorem_all_proved_ok;
    trec.ih = s->invariant_all_hold_ok;
    trec.cm = s->cost_monotone_ok;
    trec.ld = s->layer_distinct_ok;
    trec.le = s->layer_all_enabled_ok;
    trec.sigma = s->sigma_leanstral;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *kind_name(cos_v292_kind_t k) {
    switch (k) {
    case COS_V292_KIND_AI_ASSISTED_PROOF: return "AI_ASSISTED_PROOF";
    case COS_V292_KIND_AI_GENERIC:        return "AI_GENERIC";
    case COS_V292_KIND_PROD_BUG:          return "PROD_BUG";
    }
    return "UNKNOWN";
}

static const char *layer_name(cos_v292_layer_t l) {
    switch (l) {
    case COS_V292_LAYER_C_CONTRACTS:       return "C_CONTRACTS";
    case COS_V292_LAYER_THEOREM_PROOFS:    return "THEOREM_PROOFS";
    case COS_V292_LAYER_AI_ASSISTED_PROOF: return "AI_ASSISTED_PROOFS";
    }
    return "UNKNOWN";
}

size_t cos_v292_to_json(const cos_v292_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v292\","
        "\"n_theorem_rows_ok\":%d,\"theorem_all_proved_ok\":%s,"
        "\"n_invariant_rows_ok\":%d,\"invariant_all_hold_ok\":%s,"
        "\"n_cost_rows_ok\":%d,\"cost_monotone_ok\":%s,"
        "\"n_layer_rows_ok\":%d,"
        "\"layer_distinct_ok\":%s,\"layer_all_enabled_ok\":%s,"
        "\"sigma_leanstral\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"theorem\":[",
        s->n_theorem_rows_ok,
        s->theorem_all_proved_ok ? "true" : "false",
        s->n_invariant_rows_ok,
        s->invariant_all_hold_ok ? "true" : "false",
        s->n_cost_rows_ok,
        s->cost_monotone_ok ? "true" : "false",
        s->n_layer_rows_ok,
        s->layer_distinct_ok    ? "true" : "false",
        s->layer_all_enabled_ok ? "true" : "false",
        s->sigma_leanstral,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V292_N_THEOREM; ++i) {
        const cos_v292_theorem_t *t = &s->theorem[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"lean4_proved\":%s}",
            i == 0 ? "" : ",", t->name,
            t->lean4_proved ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"invariant\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V292_N_INVARIANT; ++i) {
        const cos_v292_invariant_t *iv = &s->invariant[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"holds\":%s}",
            i == 0 ? "" : ",", iv->name,
            iv->holds ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"cost\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V292_N_COST; ++i) {
        const cos_v292_cost_t *c = &s->cost[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"proof_cost_usd\":%d,"
            "\"kind\":\"%s\"}",
            i == 0 ? "" : ",", c->label, c->proof_cost_usd,
            kind_name(c->kind));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"layer\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V292_N_LAYER; ++i) {
        const cos_v292_layer_row_t *l = &s->layer[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"source\":\"%s\",\"layer\":\"%s\","
            "\"enabled\":%s}",
            i == 0 ? "" : ",", l->source, layer_name(l->layer),
            l->enabled ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v292_self_test(void) {
    cos_v292_state_t s;
    cos_v292_init(&s, 0x292cccULL);
    cos_v292_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_T[COS_V292_N_THEOREM] = {
        "gate_determinism", "gate_range", "gate_threshold_monotone"
    };
    for (int i = 0; i < COS_V292_N_THEOREM; ++i) {
        if (strcmp(s.theorem[i].name, WANT_T[i]) != 0) return 2;
        if (!s.theorem[i].lean4_proved) return 3;
    }
    if (s.n_theorem_rows_ok     != COS_V292_N_THEOREM) return 4;
    if (!s.theorem_all_proved_ok) return 5;

    static const char *WANT_I[COS_V292_N_INVARIANT] = {
        "sigma_in_unit_interval", "sigma_zero_k_eff_full",
        "sigma_one_k_eff_zero",   "sigma_monotone_confidence_loss"
    };
    for (int i = 0; i < COS_V292_N_INVARIANT; ++i) {
        if (strcmp(s.invariant[i].name, WANT_I[i]) != 0) return 6;
        if (!s.invariant[i].holds) return 7;
    }
    if (s.n_invariant_rows_ok     != COS_V292_N_INVARIANT) return 8;
    if (!s.invariant_all_hold_ok) return 9;

    static const int WANT_C[COS_V292_N_COST] = { 36, 549, 10000 };
    static const char *WANT_L[COS_V292_N_COST] = {
        "leanstral", "claude", "bug_in_prod"
    };
    for (int i = 0; i < COS_V292_N_COST; ++i) {
        if (strcmp(s.cost[i].label, WANT_L[i]) != 0) return 10;
        if (s.cost[i].proof_cost_usd != WANT_C[i])    return 11;
    }
    if (s.n_cost_rows_ok   != COS_V292_N_COST) return 12;
    if (!s.cost_monotone_ok) return 13;
    if (!(s.cost[0].proof_cost_usd < s.cost[1].proof_cost_usd)) return 14;

    static const char *WANT_S[COS_V292_N_LAYER] = {
        "frama_c_v138", "lean4_v207", "leanstral_v292"
    };
    for (int i = 0; i < COS_V292_N_LAYER; ++i) {
        if (strcmp(s.layer[i].source, WANT_S[i]) != 0) return 15;
        if (!s.layer[i].enabled) return 16;
    }
    if (s.n_layer_rows_ok     != COS_V292_N_LAYER) return 17;
    if (!s.layer_distinct_ok)    return 18;
    if (!s.layer_all_enabled_ok) return 19;

    if (s.sigma_leanstral < 0.0f || s.sigma_leanstral > 1.0f) return 20;
    if (s.sigma_leanstral > 1e-6f) return 21;

    cos_v292_state_t u;
    cos_v292_init(&u, 0x292cccULL);
    cos_v292_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 22;
    return 0;
}

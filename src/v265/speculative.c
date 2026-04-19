/*
 * v265 σ-Speculative — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "speculative.h"

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

static const struct { const char *name; const char *role; }
    MODELS[COS_V265_N_MODELS] = {
    { "bitnet-1.5B-local", "draft"    },
    { "airllm-70B-local",  "verifier" },
};

static const struct { const char *lbl; float lo, hi; int spec; }
    BANDS[COS_V265_N_BANDS] = {
    { "easy",    0.00f, 0.20f, 12 },
    { "medium",  0.20f, 0.40f,  8 },
    { "hard",    0.40f, 0.60f,  6 },
    { "extreme", 0.60f, 1.00f,  4 },
};

static const struct { const char *a; float sa;
                      const char *b; float sb; const char *w; }
    DUELS[COS_V265_N_DUELS] = {
    { "bitnet-A", 0.12f, "bitnet-B", 0.19f, "A" },
    { "bitnet-A", 0.28f, "bitnet-B", 0.15f, "B" },
    { "bitnet-A", 0.08f, "bitnet-B", 0.22f, "A" },
};

static const struct { const char *t; float s; }
    GATES[COS_V265_N_GATES] = {
    { "tok_the",       0.08f },   /* ACCEPT */
    { "tok_quantum",   0.24f },   /* ACCEPT */
    { "tok_hallucit",  0.48f },   /* REJECT */
    { "tok_suspect",   0.67f },   /* REJECT */
};

void cos_v265_init(cos_v265_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed     = seed ? seed : 0x265CAFEULL;
    s->tau_spec = 0.35f;
}

void cos_v265_run(cos_v265_state_t *s) {
    uint64_t prev = 0x265CAF00ULL;

    s->n_models_ok = 0;
    for (int i = 0; i < COS_V265_N_MODELS; ++i) {
        cos_v265_model_t *m = &s->models[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), MODELS[i].name);
        cpy(m->role, sizeof(m->role), MODELS[i].role);
        bool ok = (strlen(m->name) > 0) &&
                  (strcmp(m->role, "draft") == 0 ||
                   strcmp(m->role, "verifier") == 0);
        if (ok) s->n_models_ok++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(m->role, strlen(m->role), prev);
    }

    s->n_bands_ok = 0;
    for (int i = 0; i < COS_V265_N_BANDS; ++i) {
        cos_v265_band_t *b = &s->bands[i];
        memset(b, 0, sizeof(*b));
        cpy(b->label, sizeof(b->label), BANDS[i].lbl);
        b->sigma_lo = BANDS[i].lo;
        b->sigma_hi = BANDS[i].hi;
        b->spec_len = BANDS[i].spec;
        if (b->sigma_hi > b->sigma_lo && b->spec_len > 0) s->n_bands_ok++;
        prev = fnv1a(b->label, strlen(b->label), prev);
        prev = fnv1a(&b->sigma_lo, sizeof(b->sigma_lo), prev);
        prev = fnv1a(&b->sigma_hi, sizeof(b->sigma_hi), prev);
        prev = fnv1a(&b->spec_len, sizeof(b->spec_len), prev);
    }
    s->monotone_ok = true;
    for (int i = 1; i < COS_V265_N_BANDS; ++i) {
        if (s->bands[i].spec_len > s->bands[i-1].spec_len) {
            s->monotone_ok = false; break;
        }
    }

    s->n_duels_ok = 0;
    s->n_a_wins = s->n_b_wins = 0;
    for (int i = 0; i < COS_V265_N_DUELS; ++i) {
        cos_v265_duel_t *d = &s->duels[i];
        memset(d, 0, sizeof(*d));
        cpy(d->draft_a, sizeof(d->draft_a), DUELS[i].a);
        d->sigma_a = DUELS[i].sa;
        cpy(d->draft_b, sizeof(d->draft_b), DUELS[i].b);
        d->sigma_b = DUELS[i].sb;
        cpy(d->winner, sizeof(d->winner), DUELS[i].w);
        const char *expect = (d->sigma_a < d->sigma_b) ? "A" : "B";
        if (strcmp(d->winner, expect) == 0 &&
            d->sigma_a >= 0.0f && d->sigma_a <= 1.0f &&
            d->sigma_b >= 0.0f && d->sigma_b <= 1.0f)
            s->n_duels_ok++;
        if (strcmp(d->winner, "A") == 0) s->n_a_wins++;
        if (strcmp(d->winner, "B") == 0) s->n_b_wins++;
        prev = fnv1a(d->draft_a, strlen(d->draft_a), prev);
        prev = fnv1a(&d->sigma_a, sizeof(d->sigma_a), prev);
        prev = fnv1a(d->draft_b, strlen(d->draft_b), prev);
        prev = fnv1a(&d->sigma_b, sizeof(d->sigma_b), prev);
        prev = fnv1a(d->winner, strlen(d->winner), prev);
    }

    s->n_accept = s->n_reject = 0;
    for (int i = 0; i < COS_V265_N_GATES; ++i) {
        cos_v265_gate_t *g = &s->gates[i];
        memset(g, 0, sizeof(*g));
        cpy(g->token, sizeof(g->token), GATES[i].t);
        g->sigma_speculated = GATES[i].s;
        g->decision = (g->sigma_speculated <= s->tau_spec)
                          ? COS_V265_GATE_ACCEPT
                          : COS_V265_GATE_REJECT;
        if (g->decision == COS_V265_GATE_ACCEPT) s->n_accept++;
        if (g->decision == COS_V265_GATE_REJECT) s->n_reject++;
        prev = fnv1a(g->token, strlen(g->token), prev);
        prev = fnv1a(&g->sigma_speculated, sizeof(g->sigma_speculated), prev);
        prev = fnv1a(&g->decision,         sizeof(g->decision),         prev);
    }

    cos_v265_throughput_t *th = &s->throughput;
    th->tok_per_s_plain      = 45.0f;
    th->tok_per_s_sigma_spec = 108.0f;
    th->speedup_x            =
        th->tok_per_s_sigma_spec / th->tok_per_s_plain;
    th->speedup_ok =
        (th->tok_per_s_sigma_spec > th->tok_per_s_plain) &&
        (th->speedup_x >= 2.0f);
    prev = fnv1a(&th->tok_per_s_plain,      sizeof(th->tok_per_s_plain),      prev);
    prev = fnv1a(&th->tok_per_s_sigma_spec, sizeof(th->tok_per_s_sigma_spec), prev);
    prev = fnv1a(&th->speedup_x,            sizeof(th->speedup_x),            prev);

    int total   = 2 + 4 + 1 + 3 + 1 + 4 + 1 + 1;
    int passing = s->n_models_ok + s->n_bands_ok +
                  (s->monotone_ok ? 1 : 0) +
                  s->n_duels_ok +
                  ((s->n_a_wins >= 1 && s->n_b_wins >= 1) ? 1 : 0) +
                  (s->n_accept + s->n_reject) +
                  ((s->n_accept >= 1 && s->n_reject >= 1) ? 1 : 0) +
                  (th->speedup_ok ? 1 : 0);
    s->sigma_speculative = 1.0f - ((float)passing / (float)total);
    if (s->sigma_speculative < 0.0f) s->sigma_speculative = 0.0f;
    if (s->sigma_speculative > 1.0f) s->sigma_speculative = 1.0f;

    struct { int nm, nb, nd, naw, nbw, na, nr;
             bool mo, sp; float sigma, tau;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nm = s->n_models_ok;
    trec.nb = s->n_bands_ok;
    trec.nd = s->n_duels_ok;
    trec.naw = s->n_a_wins;
    trec.nbw = s->n_b_wins;
    trec.na = s->n_accept;
    trec.nr = s->n_reject;
    trec.mo = s->monotone_ok;
    trec.sp = th->speedup_ok;
    trec.sigma = s->sigma_speculative;
    trec.tau = s->tau_spec;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *gate_name(cos_v265_gate_dec_t d) {
    switch (d) {
    case COS_V265_GATE_ACCEPT: return "ACCEPT";
    case COS_V265_GATE_REJECT: return "REJECT";
    }
    return "UNKNOWN";
}

size_t cos_v265_to_json(const cos_v265_state_t *s, char *buf, size_t cap) {
    const cos_v265_throughput_t *th = &s->throughput;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v265\","
        "\"tau_spec\":%.3f,"
        "\"n_models_ok\":%d,\"n_bands_ok\":%d,"
        "\"monotone_ok\":%s,"
        "\"n_duels_ok\":%d,\"n_a_wins\":%d,\"n_b_wins\":%d,"
        "\"n_accept\":%d,\"n_reject\":%d,"
        "\"throughput\":{\"tok_per_s_plain\":%.3f,"
        "\"tok_per_s_sigma_spec\":%.3f,"
        "\"speedup_x\":%.4f,\"speedup_ok\":%s},"
        "\"sigma_speculative\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"models\":[",
        s->tau_spec,
        s->n_models_ok, s->n_bands_ok,
        s->monotone_ok ? "true" : "false",
        s->n_duels_ok, s->n_a_wins, s->n_b_wins,
        s->n_accept, s->n_reject,
        th->tok_per_s_plain, th->tok_per_s_sigma_spec,
        th->speedup_x, th->speedup_ok ? "true" : "false",
        s->sigma_speculative,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V265_N_MODELS; ++i) {
        const cos_v265_model_t *m = &s->models[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"role\":\"%s\"}",
            i == 0 ? "" : ",", m->name, m->role);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"bands\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V265_N_BANDS; ++i) {
        const cos_v265_band_t *b = &s->bands[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_lo\":%.4f,"
            "\"sigma_hi\":%.4f,\"spec_len\":%d}",
            i == 0 ? "" : ",", b->label, b->sigma_lo,
            b->sigma_hi, b->spec_len);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"duels\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V265_N_DUELS; ++i) {
        const cos_v265_duel_t *d = &s->duels[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"draft_a\":\"%s\",\"sigma_a\":%.4f,"
            "\"draft_b\":\"%s\",\"sigma_b\":%.4f,"
            "\"winner\":\"%s\"}",
            i == 0 ? "" : ",", d->draft_a, d->sigma_a,
            d->draft_b, d->sigma_b, d->winner);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"gates\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V265_N_GATES; ++i) {
        const cos_v265_gate_t *g = &s->gates[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"token\":\"%s\",\"sigma_speculated\":%.4f,"
            "\"decision\":\"%s\"}",
            i == 0 ? "" : ",", g->token,
            g->sigma_speculated, gate_name(g->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v265_self_test(void) {
    cos_v265_state_t s;
    cos_v265_init(&s, 0x265CAFEULL);
    cos_v265_run(&s);
    if (!s.chain_valid) return 1;

    if (strcmp(s.models[0].role, "draft")    != 0) return 2;
    if (strcmp(s.models[1].role, "verifier") != 0) return 3;
    if (s.n_models_ok != COS_V265_N_MODELS) return 4;

    const int want_spec[COS_V265_N_BANDS] = { 12, 8, 6, 4 };
    for (int i = 0; i < COS_V265_N_BANDS; ++i) {
        if (s.bands[i].spec_len != want_spec[i]) return 5;
        if (s.bands[i].sigma_hi <= s.bands[i].sigma_lo) return 6;
    }
    if (s.n_bands_ok != COS_V265_N_BANDS) return 7;
    if (!s.monotone_ok) return 8;

    for (int i = 0; i < COS_V265_N_DUELS; ++i) {
        const char *expect =
            (s.duels[i].sigma_a < s.duels[i].sigma_b) ? "A" : "B";
        if (strcmp(s.duels[i].winner, expect) != 0) return 9;
    }
    if (s.n_duels_ok != COS_V265_N_DUELS) return 10;
    if (s.n_a_wins < 1) return 11;
    if (s.n_b_wins < 1) return 12;

    for (int i = 0; i < COS_V265_N_GATES; ++i) {
        cos_v265_gate_dec_t expect =
            (s.gates[i].sigma_speculated <= s.tau_spec)
                ? COS_V265_GATE_ACCEPT
                : COS_V265_GATE_REJECT;
        if (s.gates[i].decision != expect) return 13;
    }
    if (s.n_accept < 1) return 14;
    if (s.n_reject < 1) return 15;
    if (s.n_accept + s.n_reject != COS_V265_N_GATES) return 16;

    if (s.throughput.tok_per_s_plain >= s.throughput.tok_per_s_sigma_spec) return 17;
    if (s.throughput.speedup_x < 2.0f) return 18;
    if (!s.throughput.speedup_ok) return 19;

    if (s.sigma_speculative < 0.0f || s.sigma_speculative > 1.0f) return 20;
    if (s.sigma_speculative > 1e-6f) return 21;

    cos_v265_state_t t;
    cos_v265_init(&t, 0x265CAFEULL);
    cos_v265_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 22;
    return 0;
}

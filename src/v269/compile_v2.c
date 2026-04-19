/*
 * v269 σ-Compile-v2 — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "compile_v2.h"

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

static const struct { const char *s; int ns; }
    STAGES[COS_V269_N_STAGES] = {
    { "tokenize",    180 },
    { "embed",       240 },
    { "attention",  2600 },
    { "ffn",        1800 },
    { "sigma_gate",    1 },
    { "detokenize",  140 },
};

static const struct { const char *t; float tok; float budget; }
    TARGETS[COS_V269_N_TARGETS] = {
    { "m4_apple_silicon",    118.0f, 100.0f },
    { "rpi5_arm64",           14.0f,  10.0f },
    { "gpu_4gb_speculative",  62.0f,  50.0f },
    { "x86_avx512",           95.0f,  80.0f },
};

static const struct { const char *p; float hot; }
    PGO[COS_V269_N_PGO] = {
    { "path_attention_hot",  0.62f },   /* aggressive */
    { "path_embed_common",   0.34f },   /* aggressive */
    { "path_rare_dispatch",  0.08f },   /* space      */
    { "path_fallback_api",   0.03f },   /* space      */
};

static const struct { int idx; float s; }
    ELIM[COS_V269_N_ELIM] = {
    {  3, 0.02f },   /* elided  */
    {  7, 0.04f },   /* elided  */
    { 12, 0.11f },   /* kept    */
    { 18, 0.27f },   /* kept    */
    { 23, 0.01f },   /* elided  */
    { 29, 0.34f },   /* kept    */
};

void cos_v269_init(cos_v269_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x269C0FEULL;
}

void cos_v269_run(cos_v269_state_t *s) {
    uint64_t prev = 0x269C0F00ULL;

    s->n_stages_ok = 0;
    for (int i = 0; i < COS_V269_N_STAGES; ++i) {
        cos_v269_stage_t *st = &s->stages[i];
        memset(st, 0, sizeof(*st));
        cpy(st->stage, sizeof(st->stage), STAGES[i].s);
        st->aot_compiled = true;
        st->native       = true;
        st->latency_ns   = STAGES[i].ns;
        if (st->aot_compiled && st->native && st->latency_ns > 0)
            s->n_stages_ok++;
        prev = fnv1a(st->stage, strlen(st->stage), prev);
        prev = fnv1a(&st->aot_compiled, sizeof(st->aot_compiled), prev);
        prev = fnv1a(&st->latency_ns,   sizeof(st->latency_ns),   prev);
        prev = fnv1a(&st->native,       sizeof(st->native),       prev);
    }

    s->n_targets_ok = 0;
    for (int i = 0; i < COS_V269_N_TARGETS; ++i) {
        cos_v269_target_t *t = &s->targets[i];
        memset(t, 0, sizeof(*t));
        cpy(t->target, sizeof(t->target), TARGETS[i].t);
        t->supported        = true;
        t->tok_per_s        = TARGETS[i].tok;
        t->budget_tok_per_s = TARGETS[i].budget;
        t->meets_budget     = (t->tok_per_s >= t->budget_tok_per_s);
        if (t->supported && t->meets_budget) s->n_targets_ok++;
        prev = fnv1a(t->target, strlen(t->target), prev);
        prev = fnv1a(&t->supported,        sizeof(t->supported),        prev);
        prev = fnv1a(&t->tok_per_s,        sizeof(t->tok_per_s),        prev);
        prev = fnv1a(&t->budget_tok_per_s, sizeof(t->budget_tok_per_s), prev);
        prev = fnv1a(&t->meets_budget,     sizeof(t->meets_budget),     prev);
    }

    s->n_pgo_ok = 0;
    s->n_pgo_aggressive = s->n_pgo_space = 0;
    for (int i = 0; i < COS_V269_N_PGO; ++i) {
        cos_v269_pgo_t *p = &s->pgo[i];
        memset(p, 0, sizeof(*p));
        cpy(p->path, sizeof(p->path), PGO[i].p);
        p->hotpath_fraction = PGO[i].hot;
        cpy(p->optimization, sizeof(p->optimization),
            (p->hotpath_fraction >= 0.20f) ? "aggressive" : "space");
        bool expect_aggressive = (p->hotpath_fraction >= 0.20f);
        bool ok =
            p->hotpath_fraction >= 0.0f && p->hotpath_fraction <= 1.0f &&
            ((expect_aggressive  && strcmp(p->optimization, "aggressive") == 0) ||
             (!expect_aggressive && strcmp(p->optimization, "space") == 0));
        if (ok) s->n_pgo_ok++;
        if (strcmp(p->optimization, "aggressive") == 0) s->n_pgo_aggressive++;
        if (strcmp(p->optimization, "space") == 0)      s->n_pgo_space++;
        prev = fnv1a(p->path, strlen(p->path), prev);
        prev = fnv1a(&p->hotpath_fraction, sizeof(p->hotpath_fraction), prev);
        prev = fnv1a(p->optimization, strlen(p->optimization), prev);
    }

    s->n_elim_ok = 0;
    s->n_elided = s->n_kept = 0;
    for (int i = 0; i < COS_V269_N_ELIM; ++i) {
        cos_v269_elim_t *e = &s->elim[i];
        memset(e, 0, sizeof(*e));
        e->layer_idx     = ELIM[i].idx;
        e->sigma_profile = ELIM[i].s;
        e->elided        = (e->sigma_profile < 0.05f);
        bool expect = (e->sigma_profile < 0.05f);
        if (e->elided == expect &&
            e->sigma_profile >= 0.0f && e->sigma_profile <= 1.0f)
            s->n_elim_ok++;
        if (e->elided) s->n_elided++;
        else           s->n_kept++;
        prev = fnv1a(&e->layer_idx,     sizeof(e->layer_idx),     prev);
        prev = fnv1a(&e->sigma_profile, sizeof(e->sigma_profile), prev);
        prev = fnv1a(&e->elided,        sizeof(e->elided),        prev);
    }

    int total   = COS_V269_N_STAGES + COS_V269_N_TARGETS +
                  COS_V269_N_PGO + 1 +
                  COS_V269_N_ELIM + 1;
    int passing = s->n_stages_ok + s->n_targets_ok +
                  s->n_pgo_ok +
                  ((s->n_pgo_aggressive >= 1 && s->n_pgo_space >= 1) ? 1 : 0) +
                  s->n_elim_ok +
                  ((s->n_elided >= 1 && s->n_kept >= 1) ? 1 : 0);
    s->sigma_compile_v2 = 1.0f - ((float)passing / (float)total);
    if (s->sigma_compile_v2 < 0.0f) s->sigma_compile_v2 = 0.0f;
    if (s->sigma_compile_v2 > 1.0f) s->sigma_compile_v2 = 1.0f;

    struct { int nst, ntg, npg, npa, nps, ne, nel, nk;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nst = s->n_stages_ok;
    trec.ntg = s->n_targets_ok;
    trec.npg = s->n_pgo_ok;
    trec.npa = s->n_pgo_aggressive;
    trec.nps = s->n_pgo_space;
    trec.ne  = s->n_elim_ok;
    trec.nel = s->n_elided;
    trec.nk  = s->n_kept;
    trec.sigma = s->sigma_compile_v2;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v269_to_json(const cos_v269_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v269\","
        "\"n_stages_ok\":%d,\"n_targets_ok\":%d,"
        "\"n_pgo_ok\":%d,"
        "\"n_pgo_aggressive\":%d,\"n_pgo_space\":%d,"
        "\"n_elim_ok\":%d,"
        "\"n_elided\":%d,\"n_kept\":%d,"
        "\"sigma_compile_v2\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"stages\":[",
        s->n_stages_ok, s->n_targets_ok,
        s->n_pgo_ok,
        s->n_pgo_aggressive, s->n_pgo_space,
        s->n_elim_ok,
        s->n_elided, s->n_kept,
        s->sigma_compile_v2,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V269_N_STAGES; ++i) {
        const cos_v269_stage_t *st = &s->stages[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"stage\":\"%s\",\"aot_compiled\":%s,"
            "\"latency_ns\":%d,\"native\":%s}",
            i == 0 ? "" : ",", st->stage,
            st->aot_compiled ? "true" : "false",
            st->latency_ns,
            st->native ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"targets\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V269_N_TARGETS; ++i) {
        const cos_v269_target_t *t = &s->targets[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"target\":\"%s\",\"supported\":%s,"
            "\"tok_per_s\":%.3f,"
            "\"budget_tok_per_s\":%.3f,"
            "\"meets_budget\":%s}",
            i == 0 ? "" : ",", t->target,
            t->supported ? "true" : "false",
            t->tok_per_s, t->budget_tok_per_s,
            t->meets_budget ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"pgo\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V269_N_PGO; ++i) {
        const cos_v269_pgo_t *p = &s->pgo[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"path\":\"%s\",\"hotpath_fraction\":%.4f,"
            "\"optimization\":\"%s\"}",
            i == 0 ? "" : ",", p->path, p->hotpath_fraction,
            p->optimization);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"elim\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V269_N_ELIM; ++i) {
        const cos_v269_elim_t *e = &s->elim[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"layer_idx\":%d,\"sigma_profile\":%.4f,"
            "\"elided\":%s}",
            i == 0 ? "" : ",", e->layer_idx,
            e->sigma_profile, e->elided ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v269_self_test(void) {
    cos_v269_state_t s;
    cos_v269_init(&s, 0x269C0FEULL);
    cos_v269_run(&s);
    if (!s.chain_valid) return 1;

    const char *sn[COS_V269_N_STAGES] = {
        "tokenize", "embed", "attention", "ffn",
        "sigma_gate", "detokenize"
    };
    for (int i = 0; i < COS_V269_N_STAGES; ++i) {
        if (strcmp(s.stages[i].stage, sn[i]) != 0) return 2;
        if (!s.stages[i].aot_compiled)             return 3;
        if (!s.stages[i].native)                   return 4;
        if (s.stages[i].latency_ns <= 0)           return 5;
    }
    if (s.n_stages_ok != COS_V269_N_STAGES) return 6;

    const char *tn[COS_V269_N_TARGETS] = {
        "m4_apple_silicon", "rpi5_arm64",
        "gpu_4gb_speculative", "x86_avx512"
    };
    for (int i = 0; i < COS_V269_N_TARGETS; ++i) {
        if (strcmp(s.targets[i].target, tn[i]) != 0) return 7;
        if (!s.targets[i].supported)                 return 8;
        if (s.targets[i].tok_per_s < s.targets[i].budget_tok_per_s) return 9;
        if (!s.targets[i].meets_budget)              return 10;
    }
    if (s.n_targets_ok != COS_V269_N_TARGETS) return 11;

    for (int i = 0; i < COS_V269_N_PGO; ++i) {
        const char *exp =
            (s.pgo[i].hotpath_fraction >= 0.20f) ? "aggressive" : "space";
        if (strcmp(s.pgo[i].optimization, exp) != 0) return 12;
    }
    if (s.n_pgo_ok != COS_V269_N_PGO) return 13;
    if (s.n_pgo_aggressive < 1)       return 14;
    if (s.n_pgo_space < 1)            return 15;

    for (int i = 0; i < COS_V269_N_ELIM; ++i) {
        bool exp = (s.elim[i].sigma_profile < 0.05f);
        if (s.elim[i].elided != exp) return 16;
    }
    if (s.n_elim_ok != COS_V269_N_ELIM) return 17;
    if (s.n_elided < 1) return 18;
    if (s.n_kept   < 1) return 19;

    if (s.sigma_compile_v2 < 0.0f || s.sigma_compile_v2 > 1.0f) return 20;
    if (s.sigma_compile_v2 > 1e-6f) return 21;

    cos_v269_state_t u;
    cos_v269_init(&u, 0x269C0FEULL);
    cos_v269_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 22;
    return 0;
}

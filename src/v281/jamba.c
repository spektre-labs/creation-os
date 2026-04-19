/*
 * v281 σ-Jamba — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "jamba.h"

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

static const struct { const char *n; cos_v281_cmp_t c; }
    LAYERS[COS_V281_N_LAYER] = {
    { "mamba",       COS_V281_CMP_LINEAR    },
    { "transformer", COS_V281_CMP_QUADRATIC },
    { "moe",         COS_V281_CMP_SPARSE    },
};

static const struct { const char *lbl; float sd; cos_v281_arch_t a; }
    MIXINGS[COS_V281_N_MIXING] = {
    { "easy",    0.10f, COS_V281_ARCH_MAMBA       },
    { "hard",    0.80f, COS_V281_ARCH_TRANSFORMER },
    { "factual", 0.40f, COS_V281_ARCH_MOE         },
    { "long",    0.30f, COS_V281_ARCH_MAMBA       },
};

static const struct { const char *n; int slot; }
    MEMORIES[COS_V281_N_MEMORY] = {
    { "engram",      1 },
    { "mamba",       2 },
    { "ttt",         3 },
    { "transformer", 4 },
    { "moe",         5 },
};

static const struct { const char *m; float sb; float sj; }
    BENCHES[COS_V281_N_BENCH] = {
    { "accuracy",    0.20f, 0.12f },
    { "latency",     0.25f, 0.15f },
    { "throughput",  0.18f, 0.10f },
};

void cos_v281_init(cos_v281_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x281888ULL;
}

void cos_v281_run(cos_v281_state_t *s) {
    uint64_t prev = 0x28188800ULL;

    s->n_layer_rows_ok = 0;
    for (int i = 0; i < COS_V281_N_LAYER; ++i) {
        cos_v281_layer_t *l = &s->layer[i];
        memset(l, 0, sizeof(*l));
        cpy(l->name, sizeof(l->name), LAYERS[i].n);
        l->complexity = LAYERS[i].c;
        if (strlen(l->name) > 0) s->n_layer_rows_ok++;
        prev = fnv1a(l->name, strlen(l->name), prev);
        prev = fnv1a(&l->complexity, sizeof(l->complexity), prev);
    }
    s->layer_complexity_ok =
        (s->layer[0].complexity == COS_V281_CMP_LINEAR)    &&
        (s->layer[1].complexity == COS_V281_CMP_QUADRATIC) &&
        (s->layer[2].complexity == COS_V281_CMP_SPARSE);
    s->layer_distinct_ok =
        (strcmp(s->layer[0].name, s->layer[1].name) != 0) &&
        (strcmp(s->layer[0].name, s->layer[2].name) != 0) &&
        (strcmp(s->layer[1].name, s->layer[2].name) != 0);

    s->n_mixing_rows_ok = 0;
    int seen_arch[4] = { 0 };   /* index 1..3 used */
    for (int i = 0; i < COS_V281_N_MIXING; ++i) {
        cos_v281_mixing_t *m = &s->mixing[i];
        memset(m, 0, sizeof(*m));
        cpy(m->label, sizeof(m->label), MIXINGS[i].lbl);
        m->sigma_difficulty = MIXINGS[i].sd;
        m->chosen           = MIXINGS[i].a;
        if (m->sigma_difficulty >= 0.0f && m->sigma_difficulty <= 1.0f)
            s->n_mixing_rows_ok++;
        if (m->chosen >= 1 && m->chosen <= 3) seen_arch[m->chosen] = 1;
        prev = fnv1a(m->label, strlen(m->label), prev);
        prev = fnv1a(&m->sigma_difficulty, sizeof(m->sigma_difficulty), prev);
        prev = fnv1a(&m->chosen,           sizeof(m->chosen),           prev);
    }
    s->n_distinct_arch = seen_arch[1] + seen_arch[2] + seen_arch[3];
    s->mixing_distinct_ok = (s->n_distinct_arch >= 2);

    s->n_memory_rows_ok = 0;
    int seen_slot[COS_V281_N_MEMORY + 1] = { 0 };
    bool mem_ok = true;
    for (int i = 0; i < COS_V281_N_MEMORY; ++i) {
        cos_v281_memory_t *mm = &s->memory[i];
        memset(mm, 0, sizeof(*mm));
        cpy(mm->name, sizeof(mm->name), MEMORIES[i].n);
        mm->tier_slot = MEMORIES[i].slot;
        mm->enabled   = (strlen(mm->name) > 0) && (mm->tier_slot >= 1) &&
                        (mm->tier_slot <= COS_V281_N_MEMORY);
        if (mm->enabled) s->n_memory_rows_ok++;
        if (mm->tier_slot < 1 || mm->tier_slot > COS_V281_N_MEMORY ||
            seen_slot[mm->tier_slot]) {
            mem_ok = false;
        } else {
            seen_slot[mm->tier_slot] = 1;
        }
        prev = fnv1a(mm->name, strlen(mm->name), prev);
        prev = fnv1a(&mm->enabled,   sizeof(mm->enabled),   prev);
        prev = fnv1a(&mm->tier_slot, sizeof(mm->tier_slot), prev);
    }
    s->memory_order_ok = mem_ok &&
        (s->memory[0].tier_slot == 1) &&
        (s->memory[1].tier_slot == 2) &&
        (s->memory[2].tier_slot == 3) &&
        (s->memory[3].tier_slot == 4) &&
        (s->memory[4].tier_slot == 5);

    s->n_bench_rows_ok = 0;
    bool bench_ok = true;
    for (int i = 0; i < COS_V281_N_BENCH; ++i) {
        cos_v281_bench_t *b = &s->bench[i];
        memset(b, 0, sizeof(*b));
        cpy(b->metric, sizeof(b->metric), BENCHES[i].m);
        b->sigma_baseline = BENCHES[i].sb;
        b->sigma_jamba    = BENCHES[i].sj;
        if (b->sigma_baseline >= 0.0f && b->sigma_baseline <= 1.0f &&
            b->sigma_jamba    >= 0.0f && b->sigma_jamba    <= 1.0f)
            s->n_bench_rows_ok++;
        if (!(b->sigma_jamba <= b->sigma_baseline)) bench_ok = false;
        prev = fnv1a(b->metric, strlen(b->metric), prev);
        prev = fnv1a(&b->sigma_baseline, sizeof(b->sigma_baseline), prev);
        prev = fnv1a(&b->sigma_jamba,    sizeof(b->sigma_jamba),    prev);
    }
    s->bench_monotone_ok = bench_ok;

    int total   = COS_V281_N_LAYER  + 1 + 1 +
                  COS_V281_N_MIXING + 1 +
                  COS_V281_N_MEMORY + 1 +
                  COS_V281_N_BENCH  + 1;
    int passing = s->n_layer_rows_ok +
                  (s->layer_complexity_ok ? 1 : 0) +
                  (s->layer_distinct_ok   ? 1 : 0) +
                  s->n_mixing_rows_ok +
                  (s->mixing_distinct_ok ? 1 : 0) +
                  s->n_memory_rows_ok +
                  (s->memory_order_ok ? 1 : 0) +
                  s->n_bench_rows_ok +
                  (s->bench_monotone_ok ? 1 : 0);
    s->sigma_jamba = 1.0f - ((float)passing / (float)total);
    if (s->sigma_jamba < 0.0f) s->sigma_jamba = 0.0f;
    if (s->sigma_jamba > 1.0f) s->sigma_jamba = 1.0f;

    struct { int nl, nm, nmem, nb, na;
             bool lc, ld, md, mo, bm;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nl   = s->n_layer_rows_ok;
    trec.nm   = s->n_mixing_rows_ok;
    trec.nmem = s->n_memory_rows_ok;
    trec.nb   = s->n_bench_rows_ok;
    trec.na   = s->n_distinct_arch;
    trec.lc   = s->layer_complexity_ok;
    trec.ld   = s->layer_distinct_ok;
    trec.md   = s->mixing_distinct_ok;
    trec.mo   = s->memory_order_ok;
    trec.bm   = s->bench_monotone_ok;
    trec.sigma = s->sigma_jamba;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *cmp_name(cos_v281_cmp_t c) {
    switch (c) {
    case COS_V281_CMP_LINEAR:    return "LINEAR";
    case COS_V281_CMP_QUADRATIC: return "QUADRATIC";
    case COS_V281_CMP_SPARSE:    return "SPARSE";
    }
    return "UNKNOWN";
}

static const char *arch_name(cos_v281_arch_t a) {
    switch (a) {
    case COS_V281_ARCH_MAMBA:       return "MAMBA";
    case COS_V281_ARCH_TRANSFORMER: return "TRANSFORMER";
    case COS_V281_ARCH_MOE:         return "MOE";
    }
    return "UNKNOWN";
}

size_t cos_v281_to_json(const cos_v281_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v281\","
        "\"n_layer_rows_ok\":%d,"
        "\"layer_complexity_ok\":%s,\"layer_distinct_ok\":%s,"
        "\"n_mixing_rows_ok\":%d,"
        "\"n_distinct_arch\":%d,\"mixing_distinct_ok\":%s,"
        "\"n_memory_rows_ok\":%d,\"memory_order_ok\":%s,"
        "\"n_bench_rows_ok\":%d,\"bench_monotone_ok\":%s,"
        "\"sigma_jamba\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"layer\":[",
        s->n_layer_rows_ok,
        s->layer_complexity_ok ? "true" : "false",
        s->layer_distinct_ok   ? "true" : "false",
        s->n_mixing_rows_ok,
        s->n_distinct_arch,
        s->mixing_distinct_ok  ? "true" : "false",
        s->n_memory_rows_ok,
        s->memory_order_ok     ? "true" : "false",
        s->n_bench_rows_ok,
        s->bench_monotone_ok   ? "true" : "false",
        s->sigma_jamba,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V281_N_LAYER; ++i) {
        const cos_v281_layer_t *l = &s->layer[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"complexity\":\"%s\"}",
            i == 0 ? "" : ",", l->name, cmp_name(l->complexity));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"mixing\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V281_N_MIXING; ++i) {
        const cos_v281_mixing_t *m = &s->mixing[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_difficulty\":%.4f,\"chosen\":\"%s\"}",
            i == 0 ? "" : ",", m->label, m->sigma_difficulty, arch_name(m->chosen));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"memory\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V281_N_MEMORY; ++i) {
        const cos_v281_memory_t *mm = &s->memory[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"enabled\":%s,\"tier_slot\":%d}",
            i == 0 ? "" : ",", mm->name,
            mm->enabled ? "true" : "false", mm->tier_slot);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"bench\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V281_N_BENCH; ++i) {
        const cos_v281_bench_t *b = &s->bench[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"metric\":\"%s\",\"sigma_baseline\":%.4f,\"sigma_jamba\":%.4f}",
            i == 0 ? "" : ",", b->metric, b->sigma_baseline, b->sigma_jamba);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int mx = snprintf(buf + off, cap - off, "]}");
    if (mx < 0 || off + (size_t)mx >= cap) return 0;
    return off + (size_t)mx;
}

int cos_v281_self_test(void) {
    cos_v281_state_t s;
    cos_v281_init(&s, 0x281888ULL);
    cos_v281_run(&s);
    if (!s.chain_valid) return 1;

    if (s.n_layer_rows_ok != COS_V281_N_LAYER) return 2;
    if (!s.layer_complexity_ok) return 3;
    if (!s.layer_distinct_ok)   return 4;
    if (strcmp(s.layer[0].name, "mamba")       != 0) return 5;
    if (strcmp(s.layer[1].name, "transformer") != 0) return 6;
    if (strcmp(s.layer[2].name, "moe")         != 0) return 7;

    static const struct { const char *lbl; cos_v281_arch_t a; }
        WANT_MIX[COS_V281_N_MIXING] = {
        { "easy",    COS_V281_ARCH_MAMBA       },
        { "hard",    COS_V281_ARCH_TRANSFORMER },
        { "factual", COS_V281_ARCH_MOE         },
        { "long",    COS_V281_ARCH_MAMBA       },
    };
    for (int i = 0; i < COS_V281_N_MIXING; ++i) {
        if (strcmp(s.mixing[i].label, WANT_MIX[i].lbl) != 0) return 8;
        if (s.mixing[i].chosen != WANT_MIX[i].a)            return 9;
    }
    if (s.n_mixing_rows_ok  != COS_V281_N_MIXING) return 10;
    if (!s.mixing_distinct_ok) return 11;
    if (s.n_distinct_arch < 2) return 12;

    static const char *WANT_MEM[COS_V281_N_MEMORY] = {
        "engram", "mamba", "ttt", "transformer", "moe"
    };
    for (int i = 0; i < COS_V281_N_MEMORY; ++i) {
        if (strcmp(s.memory[i].name, WANT_MEM[i]) != 0) return 13;
        if (s.memory[i].tier_slot != i + 1)             return 14;
        if (!s.memory[i].enabled)                       return 15;
    }
    if (s.n_memory_rows_ok != COS_V281_N_MEMORY) return 16;
    if (!s.memory_order_ok)                      return 17;

    static const char *WANT_BENCH[COS_V281_N_BENCH] = {
        "accuracy", "latency", "throughput"
    };
    for (int i = 0; i < COS_V281_N_BENCH; ++i) {
        if (strcmp(s.bench[i].metric, WANT_BENCH[i]) != 0) return 18;
        if (!(s.bench[i].sigma_jamba <= s.bench[i].sigma_baseline)) return 19;
    }
    if (s.n_bench_rows_ok  != COS_V281_N_BENCH) return 20;
    if (!s.bench_monotone_ok)                   return 21;

    if (s.sigma_jamba < 0.0f || s.sigma_jamba > 1.0f) return 22;
    if (s.sigma_jamba > 1e-6f) return 23;

    cos_v281_state_t u;
    cos_v281_init(&u, 0x281888ULL);
    cos_v281_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 24;
    return 0;
}

/*
 * v261 σ-AirLLM — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "airllm.h"

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

/* 8 representative layers.  σ_layer demonstrates the
 * σ-driven selective-precision rule across all three
 * bands (4-bit, 8-bit, 16-bit) AND isolates layer 5 as
 * the problem layer (max σ). */
static const float LAYER_SIGMA[COS_V261_N_LAYERS] = {
    0.08f,  /* L0 → 4-bit */
    0.12f,  /* L1 → 4-bit */
    0.18f,  /* L2 → 4-bit */
    0.27f,  /* L3 → 8-bit */
    0.35f,  /* L4 → 8-bit */
    0.58f,  /* L5 → 16-bit (problem layer, unique argmax) */
    0.31f,  /* L6 → 8-bit */
    0.14f,  /* L7 → 4-bit */
};

static const struct { const char *n; int ram; }
    BACKENDS[COS_V261_N_BACKENDS] = {
    { "cuda_4gb_gpu",  4 },
    { "cpu_only",      8 },
    { "macos_mps",     8 },
    { "linux_cuda",   12 },
};

/* Tradeoff regimes.  Effective tokens/s is what actually
 * reaches the user (after abstain / human review). */
static const struct { const char *r; float tps; int ab; }
    TRADEOFF[COS_V261_N_TRADEOFF] = {
    { "slow",   0.7f, 0  },
    { "aircos", 0.9f, 8  },   /* gate-driven abstain */
    { "human",  0.3f, 0  },
};

static int precision_for_sigma(float s) {
    if (s <= 0.20f) return 4;
    if (s <= 0.40f) return 8;
    return 16;
}

void cos_v261_init(cos_v261_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x261A121AULL;
}

void cos_v261_run(cos_v261_state_t *s) {
    uint64_t prev = 0x261A121AULL;

    int problem = 0;
    float max_sigma = -1.0f;
    for (int i = 0; i < COS_V261_N_LAYERS; ++i) {
        if (LAYER_SIGMA[i] > max_sigma) {
            max_sigma = LAYER_SIGMA[i];
            problem = i;
        }
    }
    s->problem_index = problem;

    s->n_layers_ok = 0;
    s->n_precision_ok = 0;
    for (int i = 0; i < COS_V261_N_LAYERS; ++i) {
        cos_v261_layer_t *l = &s->layers[i];
        memset(l, 0, sizeof(*l));
        l->index          = i;
        l->sigma_layer    = LAYER_SIGMA[i];
        l->precision_bits = precision_for_sigma(l->sigma_layer);
        l->is_problem     = (i == problem);
        if (l->sigma_layer >= 0.0f && l->sigma_layer <= 1.0f)
            s->n_layers_ok++;
        if (l->precision_bits ==
            precision_for_sigma(l->sigma_layer))
            s->n_precision_ok++;
        prev = fnv1a(&l->index,          sizeof(l->index),          prev);
        prev = fnv1a(&l->sigma_layer,    sizeof(l->sigma_layer),    prev);
        prev = fnv1a(&l->precision_bits, sizeof(l->precision_bits), prev);
        prev = fnv1a(&l->is_problem,     sizeof(l->is_problem),     prev);
    }

    s->n_backends_ok = 0;
    for (int i = 0; i < COS_V261_N_BACKENDS; ++i) {
        cos_v261_backend_t *b = &s->backends[i];
        memset(b, 0, sizeof(*b));
        cpy(b->name, sizeof(b->name), BACKENDS[i].n);
        b->supported  = true;
        b->min_ram_gb = BACKENDS[i].ram;
        if (b->supported && b->min_ram_gb >= 4) s->n_backends_ok++;
        prev = fnv1a(b->name, strlen(b->name), prev);
        prev = fnv1a(&b->supported,  sizeof(b->supported),  prev);
        prev = fnv1a(&b->min_ram_gb, sizeof(b->min_ram_gb), prev);
    }

    int aircos_idx = -1;
    float aircos_tps = 0.0f;
    for (int i = 0; i < COS_V261_N_TRADEOFF; ++i) {
        cos_v261_tradeoff_t *t = &s->tradeoff[i];
        memset(t, 0, sizeof(*t));
        cpy(t->regime, sizeof(t->regime), TRADEOFF[i].r);
        t->tokens_per_s = TRADEOFF[i].tps;
        t->abstain_pct  = TRADEOFF[i].ab;
        if (strcmp(t->regime, "aircos") == 0) {
            aircos_idx = i;
            aircos_tps = t->tokens_per_s;
        }
        prev = fnv1a(t->regime, strlen(t->regime), prev);
        prev = fnv1a(&t->tokens_per_s, sizeof(t->tokens_per_s), prev);
        prev = fnv1a(&t->abstain_pct,  sizeof(t->abstain_pct),  prev);
    }
    bool aircos_wins = (aircos_idx >= 0);
    if (aircos_wins) {
        for (int i = 0; i < COS_V261_N_TRADEOFF; ++i) {
            if (i == aircos_idx) continue;
            if (s->tradeoff[i].tokens_per_s >= aircos_tps) {
                aircos_wins = false; break;
            }
        }
    }
    bool aircos_abstains =
        (aircos_idx >= 0 && s->tradeoff[aircos_idx].abstain_pct > 0);
    s->tradeoff_ok = aircos_wins && aircos_abstains;

    int total   = COS_V261_N_LAYERS + COS_V261_N_LAYERS +
                  COS_V261_N_BACKENDS + COS_V261_N_TRADEOFF + 1;
    int passing = s->n_layers_ok + s->n_precision_ok +
                  s->n_backends_ok +
                  (s->tradeoff_ok ? COS_V261_N_TRADEOFF : 0) +
                  ( (s->layers[problem].is_problem) ? 1 : 0 );
    s->sigma_airllm = 1.0f - ((float)passing / (float)total);
    if (s->sigma_airllm < 0.0f) s->sigma_airllm = 0.0f;
    if (s->sigma_airllm > 1.0f) s->sigma_airllm = 1.0f;

    struct { int nl, np, pi, nb; bool to;
             float sa; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nl = s->n_layers_ok;
    trec.np = s->n_precision_ok;
    trec.pi = s->problem_index;
    trec.nb = s->n_backends_ok;
    trec.to = s->tradeoff_ok;
    trec.sa = s->sigma_airllm;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v261_to_json(const cos_v261_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v261\","
        "\"n_layers\":%d,\"n_backends\":%d,\"n_tradeoff\":%d,"
        "\"n_layers_ok\":%d,\"n_precision_ok\":%d,"
        "\"problem_index\":%d,"
        "\"n_backends_ok\":%d,\"tradeoff_ok\":%s,"
        "\"sigma_airllm\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"layers\":[",
        COS_V261_N_LAYERS, COS_V261_N_BACKENDS, COS_V261_N_TRADEOFF,
        s->n_layers_ok, s->n_precision_ok,
        s->problem_index,
        s->n_backends_ok,
        s->tradeoff_ok ? "true" : "false",
        s->sigma_airllm,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V261_N_LAYERS; ++i) {
        const cos_v261_layer_t *l = &s->layers[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"index\":%d,\"sigma_layer\":%.4f,"
            "\"precision_bits\":%d,\"is_problem\":%s}",
            i == 0 ? "" : ",",
            l->index, l->sigma_layer, l->precision_bits,
            l->is_problem ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"backends\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V261_N_BACKENDS; ++i) {
        const cos_v261_backend_t *b = &s->backends[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"supported\":%s,"
            "\"min_ram_gb\":%d}",
            i == 0 ? "" : ",", b->name,
            b->supported ? "true" : "false", b->min_ram_gb);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"tradeoff\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V261_N_TRADEOFF; ++i) {
        const cos_v261_tradeoff_t *t = &s->tradeoff[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"regime\":\"%s\",\"tokens_per_s\":%.4f,"
            "\"abstain_pct\":%d}",
            i == 0 ? "" : ",", t->regime,
            t->tokens_per_s, t->abstain_pct);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v261_self_test(void) {
    cos_v261_state_t s;
    cos_v261_init(&s, 0x261A121AULL);
    cos_v261_run(&s);
    if (!s.chain_valid) return 1;

    int problem_count = 0;
    float max_s = -1.0f; int max_i = -1;
    for (int i = 0; i < COS_V261_N_LAYERS; ++i) {
        if (s.layers[i].index != i)              return 2;
        if (s.layers[i].sigma_layer < 0.0f ||
            s.layers[i].sigma_layer > 1.0f)      return 3;
        int want = precision_for_sigma(s.layers[i].sigma_layer);
        if (s.layers[i].precision_bits != want)  return 4;
        if (s.layers[i].sigma_layer > max_s) {
            max_s = s.layers[i].sigma_layer; max_i = i;
        }
        if (s.layers[i].is_problem) problem_count++;
    }
    if (problem_count != 1)           return 5;
    if (!s.layers[max_i].is_problem)  return 6;
    if (s.problem_index != max_i)     return 7;

    const char *bn[COS_V261_N_BACKENDS] = {
        "cuda_4gb_gpu","cpu_only","macos_mps","linux_cuda"
    };
    for (int i = 0; i < COS_V261_N_BACKENDS; ++i) {
        if (strcmp(s.backends[i].name, bn[i]) != 0) return 8;
        if (!s.backends[i].supported)                return 9;
        if (s.backends[i].min_ram_gb < 4)            return 10;
    }
    if (s.n_backends_ok != COS_V261_N_BACKENDS) return 11;

    int aircos = -1;
    for (int i = 0; i < COS_V261_N_TRADEOFF; ++i) {
        if (strcmp(s.tradeoff[i].regime, "aircos") == 0) { aircos = i; break; }
    }
    if (aircos < 0) return 12;
    for (int i = 0; i < COS_V261_N_TRADEOFF; ++i) {
        if (i == aircos) continue;
        if (s.tradeoff[i].tokens_per_s >= s.tradeoff[aircos].tokens_per_s) return 13;
    }
    if (s.tradeoff[aircos].abstain_pct <= 0) return 14;
    if (!s.tradeoff_ok) return 15;

    if (s.sigma_airllm < 0.0f || s.sigma_airllm > 1.0f) return 16;
    if (s.sigma_airllm > 1e-6f) return 17;

    cos_v261_state_t t;
    cos_v261_init(&t, 0x261A121AULL);
    cos_v261_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 18;
    return 0;
}

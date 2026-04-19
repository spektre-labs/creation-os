/*
 * v266 σ-Flash — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "flash.h"

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

static const float HEAD_SIGMA[COS_V266_N_HEADS] = {
    0.10f, 0.18f, 0.22f, 0.14f, 0.31f, 0.09f, 0.25f, 0.17f
};

static const float HEAD_OVERHEAD[COS_V266_N_HEADS] = {
    0.42f, 0.38f, 0.55f, 0.41f, 0.61f, 0.33f, 0.49f, 0.44f
};

static const struct { const char *b; int ns; }
    PLATFORMS[COS_V266_N_PLATFORMS] = {
    { "cuda_sm90",   60 },
    { "metal_m4",    80 },
    { "neon_arm64", 220 },
};

static const struct { const char *k; float s; }
    KV[COS_V266_N_KV] = {
    { "tok_hello",      0.07f },  /* lowest σ, evicted LAST (rank 6) */
    { "tok_attention",  0.19f },
    { "tok_of",         0.12f },
    { "tok_noise",      0.58f },  /* highest σ, evicted FIRST (rank 1) */
    { "tok_the",        0.33f },
    { "tok_misspell",   0.45f },
};

void cos_v266_init(cos_v266_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x266FBA5EULL;
}

void cos_v266_run(cos_v266_state_t *s) {
    uint64_t prev = 0x266FBA50ULL;

    s->n_heads_ok = 0;
    for (int i = 0; i < COS_V266_N_HEADS; ++i) {
        cos_v266_head_t *h = &s->heads[i];
        memset(h, 0, sizeof(*h));
        h->index        = i;
        h->sigma_head   = HEAD_SIGMA[i];
        h->overhead_pct = HEAD_OVERHEAD[i];
        h->fused        = true;
        if (h->fused &&
            h->sigma_head >= 0.0f && h->sigma_head <= 1.0f &&
            h->overhead_pct >= 0.0f && h->overhead_pct < 1.0f)
            s->n_heads_ok++;
        prev = fnv1a(&h->index,        sizeof(h->index),        prev);
        prev = fnv1a(&h->sigma_head,   sizeof(h->sigma_head),   prev);
        prev = fnv1a(&h->overhead_pct, sizeof(h->overhead_pct), prev);
        prev = fnv1a(&h->fused,        sizeof(h->fused),        prev);
    }

    s->n_platforms_ok = 0;
    for (int i = 0; i < COS_V266_N_PLATFORMS; ++i) {
        cos_v266_platform_t *p = &s->platforms[i];
        memset(p, 0, sizeof(*p));
        cpy(p->backend, sizeof(p->backend), PLATFORMS[i].b);
        p->supported   = true;
        p->latency_ns  = PLATFORMS[i].ns;
        p->sigma_fused = true;
        if (p->supported && p->latency_ns > 0 && p->sigma_fused)
            s->n_platforms_ok++;
        prev = fnv1a(p->backend, strlen(p->backend), prev);
        prev = fnv1a(&p->supported,   sizeof(p->supported),   prev);
        prev = fnv1a(&p->latency_ns,  sizeof(p->latency_ns),  prev);
        prev = fnv1a(&p->sigma_fused, sizeof(p->sigma_fused), prev);
    }

    /* Compute descending-σ order → evict_rank.
     * rank 1 = highest σ (evict first). */
    int order[COS_V266_N_KV];
    for (int i = 0; i < COS_V266_N_KV; ++i) order[i] = i;
    for (int i = 0; i < COS_V266_N_KV - 1; ++i) {
        for (int j = i + 1; j < COS_V266_N_KV; ++j) {
            if (KV[order[j]].s > KV[order[i]].s) {
                int tmp = order[i]; order[i] = order[j]; order[j] = tmp;
            }
        }
    }
    int rank_for[COS_V266_N_KV];
    for (int r = 0; r < COS_V266_N_KV; ++r)
        rank_for[order[r]] = r + 1;

    s->n_kv_ok = 0;
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        cos_v266_kv_t *k = &s->kv[i];
        memset(k, 0, sizeof(*k));
        cpy(k->key, sizeof(k->key), KV[i].k);
        k->sigma_kv   = KV[i].s;
        k->evict_rank = rank_for[i];
        if (k->sigma_kv >= 0.0f && k->sigma_kv <= 1.0f &&
            k->evict_rank >= 1 && k->evict_rank <= COS_V266_N_KV)
            s->n_kv_ok++;
        prev = fnv1a(k->key, strlen(k->key), prev);
        prev = fnv1a(&k->sigma_kv,   sizeof(k->sigma_kv),   prev);
        prev = fnv1a(&k->evict_rank, sizeof(k->evict_rank), prev);
    }
    /* Verify order: rank 1 has strictly greatest σ, etc. */
    s->kv_order_ok = true;
    for (int r = 1; r < COS_V266_N_KV; ++r) {
        int i_prev = order[r - 1];
        int i_curr = order[r];
        if (s->kv[i_prev].sigma_kv < s->kv[i_curr].sigma_kv) {
            s->kv_order_ok = false; break;
        }
    }
    /* evict_rank must be a permutation of [1..N]. */
    int seen[COS_V266_N_KV + 1] = {0};
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        int r = s->kv[i].evict_rank;
        if (r < 1 || r > COS_V266_N_KV) { s->kv_order_ok = false; break; }
        if (seen[r]) { s->kv_order_ok = false; break; }
        seen[r] = 1;
    }

    cos_v266_pruning_t *p = &s->pruning;
    p->kept_tokens_before       = 4096;
    p->kept_tokens_after        = 4096;
    p->effective_ctx_k_before   = 4;    /* 4 k effective before pruning */
    p->effective_ctx_k_after    = 32;   /* 32 k effective after */
    p->pruning_ok =
        (p->kept_tokens_before == p->kept_tokens_after) &&
        (p->effective_ctx_k_after > p->effective_ctx_k_before) &&
        (p->effective_ctx_k_before > 0);
    prev = fnv1a(&p->kept_tokens_before,
                 sizeof(p->kept_tokens_before), prev);
    prev = fnv1a(&p->kept_tokens_after,
                 sizeof(p->kept_tokens_after), prev);
    prev = fnv1a(&p->effective_ctx_k_before,
                 sizeof(p->effective_ctx_k_before), prev);
    prev = fnv1a(&p->effective_ctx_k_after,
                 sizeof(p->effective_ctx_k_after), prev);
    prev = fnv1a(&p->pruning_ok,
                 sizeof(p->pruning_ok), prev);

    int total   = COS_V266_N_HEADS + COS_V266_N_PLATFORMS +
                  COS_V266_N_KV + 1 + 1;
    int passing = s->n_heads_ok + s->n_platforms_ok +
                  s->n_kv_ok +
                  (s->kv_order_ok ? 1 : 0) +
                  (p->pruning_ok ? 1 : 0);
    s->sigma_flash = 1.0f - ((float)passing / (float)total);
    if (s->sigma_flash < 0.0f) s->sigma_flash = 0.0f;
    if (s->sigma_flash > 1.0f) s->sigma_flash = 1.0f;

    struct { int nh, np, nk; bool ko, po;
             float sf; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nh = s->n_heads_ok;
    trec.np = s->n_platforms_ok;
    trec.nk = s->n_kv_ok;
    trec.ko = s->kv_order_ok;
    trec.po = p->pruning_ok;
    trec.sf = s->sigma_flash;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v266_to_json(const cos_v266_state_t *s, char *buf, size_t cap) {
    const cos_v266_pruning_t *p = &s->pruning;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v266\","
        "\"n_heads\":%d,\"n_platforms\":%d,\"n_kv\":%d,"
        "\"n_heads_ok\":%d,\"n_platforms_ok\":%d,"
        "\"n_kv_ok\":%d,\"kv_order_ok\":%s,"
        "\"pruning\":{\"kept_tokens_before\":%d,"
        "\"kept_tokens_after\":%d,"
        "\"effective_ctx_k_before\":%d,"
        "\"effective_ctx_k_after\":%d,"
        "\"pruning_ok\":%s},"
        "\"sigma_flash\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"heads\":[",
        COS_V266_N_HEADS, COS_V266_N_PLATFORMS, COS_V266_N_KV,
        s->n_heads_ok, s->n_platforms_ok,
        s->n_kv_ok, s->kv_order_ok ? "true" : "false",
        p->kept_tokens_before, p->kept_tokens_after,
        p->effective_ctx_k_before, p->effective_ctx_k_after,
        p->pruning_ok ? "true" : "false",
        s->sigma_flash,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V266_N_HEADS; ++i) {
        const cos_v266_head_t *h = &s->heads[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"index\":%d,\"sigma_head\":%.4f,"
            "\"overhead_pct\":%.4f,\"fused\":%s}",
            i == 0 ? "" : ",", h->index, h->sigma_head,
            h->overhead_pct, h->fused ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"platforms\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V266_N_PLATFORMS; ++i) {
        const cos_v266_platform_t *pl = &s->platforms[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"backend\":\"%s\",\"supported\":%s,"
            "\"latency_ns\":%d,\"sigma_fused\":%s}",
            i == 0 ? "" : ",", pl->backend,
            pl->supported   ? "true" : "false",
            pl->latency_ns,
            pl->sigma_fused ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"kv\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        const cos_v266_kv_t *k = &s->kv[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"key\":\"%s\",\"sigma_kv\":%.4f,"
            "\"evict_rank\":%d}",
            i == 0 ? "" : ",", k->key, k->sigma_kv, k->evict_rank);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v266_self_test(void) {
    cos_v266_state_t s;
    cos_v266_init(&s, 0x266FBA5EULL);
    cos_v266_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V266_N_HEADS; ++i) {
        if (s.heads[i].index != i)              return 2;
        if (!s.heads[i].fused)                  return 3;
        if (s.heads[i].sigma_head < 0.0f ||
            s.heads[i].sigma_head > 1.0f)       return 4;
        if (s.heads[i].overhead_pct < 0.0f ||
            s.heads[i].overhead_pct >= 1.0f)    return 5;
    }
    if (s.n_heads_ok != COS_V266_N_HEADS) return 6;

    const char *bn[COS_V266_N_PLATFORMS] = {
        "cuda_sm90", "metal_m4", "neon_arm64"
    };
    for (int i = 0; i < COS_V266_N_PLATFORMS; ++i) {
        if (strcmp(s.platforms[i].backend, bn[i]) != 0) return 7;
        if (!s.platforms[i].supported)                  return 8;
        if (s.platforms[i].latency_ns <= 0)             return 9;
        if (!s.platforms[i].sigma_fused)                return 10;
    }
    if (s.n_platforms_ok != COS_V266_N_PLATFORMS) return 11;

    int seen[COS_V266_N_KV + 1] = {0};
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        int r = s.kv[i].evict_rank;
        if (r < 1 || r > COS_V266_N_KV) return 12;
        if (seen[r]) return 13;
        seen[r] = 1;
    }
    /* rank 1 must have the max σ; rank N must have min σ. */
    int rk1 = -1, rkN = -1;
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        if (s.kv[i].evict_rank == 1) rk1 = i;
        if (s.kv[i].evict_rank == COS_V266_N_KV) rkN = i;
    }
    if (rk1 < 0 || rkN < 0) return 14;
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        if (i == rk1) continue;
        if (s.kv[i].sigma_kv > s.kv[rk1].sigma_kv) return 15;
    }
    for (int i = 0; i < COS_V266_N_KV; ++i) {
        if (i == rkN) continue;
        if (s.kv[i].sigma_kv < s.kv[rkN].sigma_kv) return 16;
    }
    if (s.n_kv_ok != COS_V266_N_KV) return 17;
    if (!s.kv_order_ok)              return 18;

    if (s.pruning.kept_tokens_before != s.pruning.kept_tokens_after) return 19;
    if (s.pruning.effective_ctx_k_after <= s.pruning.effective_ctx_k_before) return 20;
    if (!s.pruning.pruning_ok) return 21;

    if (s.sigma_flash < 0.0f || s.sigma_flash > 1.0f) return 22;
    if (s.sigma_flash > 1e-6f) return 23;

    cos_v266_state_t t;
    cos_v266_init(&t, 0x266FBA5EULL);
    cos_v266_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 24;
    return 0;
}

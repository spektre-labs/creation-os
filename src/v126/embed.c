/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v126 σ-Embed — implementation (pure C, deterministic).
 *
 * Contract: the 2560-d hidden block is a BitNet-layer-15 stand-in.
 * The API is fixed; v126.1 swaps the body of
 * `cos_v126_hidden_from_text` for a v101-bridge call without
 * touching callers.
 *
 * Projection used here is a 4-slot hash shingle:
 *   For each whitespace token,
 *     h0 = SplitMix64(hash(token))
 *     for k in 0..3:
 *       h_k = SplitMix64(h_{k-1})
 *       slot = h_k mod 2560
 *       sign = (h_k >> 48) & 1 ? +1 : -1
 *       v[slot] += sign
 * Then L2-normalize v.  Pseudo-random projections are a textbook
 * low-dim sketch; the property we exploit is: texts that share
 * tokens share slots with the same signs, which correlates into a
 * positive cosine.  Unrelated texts have near-zero expected cosine.
 */
#include "embed.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cos_v126_config_defaults(cos_v126_config_t *cfg) {
    if (!cfg) return;
    cfg->sigma_weight               = COS_V126_SIGMA_WEIGHT_DEFAULT;
    cfg->memory_uncertainty_penalty = COS_V126_MEM_PENALTY_DEFAULT;
}

/* ====================================================================
 * σ_profile helpers
 * ================================================================== */

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

void cos_v126_sigma_fill(cos_v126_sigma_profile_t *p,
                         const float channels[COS_V126_SIGMA_DIM]) {
    if (!p || !channels) return;
    double log_sum = 0.0;
    int used = 0;
    for (int i = 0; i < COS_V126_SIGMA_DIM; ++i) {
        float c = clamp01(channels[i]);
        p->channels[i] = c;
        if (c > 1e-6f) {
            log_sum += log(c);
            used++;
        }
    }
    if (used == 0) { p->sigma_product = 0.0f; return; }
    p->sigma_product = (float)exp(log_sum / used);
}

/* ====================================================================
 * Hash-shingle projector
 * ================================================================== */

static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    x =  x ^ (x >> 31);
    return x;
}

static uint64_t hash_token(const char *s, size_t n) {
    uint64_t h = 1469598103934665603ULL;  /* FNV-1a 64 offset */
    for (size_t i = 0; i < n; ++i) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ULL;
    }
    return splitmix64(h);
}

void cos_v126_hidden_from_text(const char *text, float *hidden) {
    if (!hidden) return;
    memset(hidden, 0, sizeof(float) * COS_V126_HIDDEN_DIM);
    if (!text) return;

    /* Tokenize on runs of alnum (ASCII), lowercased. */
    char buf[256];
    size_t bi = 0;
    for (const unsigned char *p = (const unsigned char *)text; ; ++p) {
        int is_break = (*p == 0) ||
                       !(isalnum(*p) || *p == '_' || *p == '-');
        if (!is_break) {
            if (bi < sizeof(buf) - 1) {
                buf[bi++] = (char)tolower(*p);
            }
        } else {
            if (bi > 0) {
                uint64_t h = hash_token(buf, bi);
                /* 4-slot shingle */
                for (int k = 0; k < 4; ++k) {
                    h = splitmix64(h);
                    uint32_t slot = (uint32_t)(h % (uint64_t)COS_V126_HIDDEN_DIM);
                    int sign = ((h >> 48) & 1ULL) ? 1 : -1;
                    hidden[slot] += (float)sign;
                }
                bi = 0;
            }
            if (*p == 0) break;
        }
    }

    /* L2-normalize the hidden block. */
    double s2 = 0.0;
    for (int i = 0; i < COS_V126_HIDDEN_DIM; ++i)
        s2 += (double)hidden[i] * (double)hidden[i];
    if (s2 > 0.0) {
        float inv = (float)(1.0 / sqrt(s2));
        for (int i = 0; i < COS_V126_HIDDEN_DIM; ++i)
            hidden[i] *= inv;
    }
}

/* ====================================================================
 * Compose + cosine + score + rank
 * ================================================================== */

void cos_v126_embed(const cos_v126_config_t *cfg,
                    const char *text,
                    const cos_v126_sigma_profile_t *sigma,
                    cos_v126_embedding_t *out) {
    if (!cfg || !out) return;
    cos_v126_hidden_from_text(text, out->v);
    float w = cfg->sigma_weight;
    for (int i = 0; i < COS_V126_SIGMA_DIM; ++i) {
        float c = sigma ? clamp01(sigma->channels[i]) : 0.0f;
        out->v[COS_V126_HIDDEN_DIM + i] = w * c;
    }
}

float cos_v126_cosine(const cos_v126_embedding_t *a,
                      const cos_v126_embedding_t *b) {
    if (!a || !b) return 0.0f;
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (int i = 0; i < COS_V126_EMBED_DIM; ++i) {
        dot += (double)a->v[i] * (double)b->v[i];
        na  += (double)a->v[i] * (double)a->v[i];
        nb  += (double)b->v[i] * (double)b->v[i];
    }
    if (na == 0.0 || nb == 0.0) return 0.0f;
    return (float)(dot / (sqrt(na) * sqrt(nb)));
}

float cos_v126_score(const cos_v126_config_t *cfg,
                     const cos_v126_embedding_t *q,
                     const cos_v126_embedding_t *m,
                     const cos_v126_sigma_profile_t *ms) {
    if (!cfg || !q || !m) return 0.0f;
    float c = cos_v126_cosine(q, m);
    float sp = ms ? clamp01(ms->sigma_product) : 0.0f;
    return c / (1.0f + cfg->memory_uncertainty_penalty * sp);
}

int cos_v126_rank_topk(const cos_v126_config_t *cfg,
                       const cos_v126_embedding_t *q,
                       const cos_v126_embedding_t *memories,
                       const cos_v126_sigma_profile_t *m_sigmas,
                       int n,
                       int *out_idx,
                       float *out_scores,
                       int k) {
    if (!cfg || !q || !memories || !out_idx || n <= 0 || k <= 0)
        return 0;
    if (k > n) k = n;

    /* O(n·k) selection — memory retrieval n's are typically small. */
    float *score = (float*)malloc(sizeof(float) * (size_t)n);
    int   *used  = (int*)  calloc((size_t)n, sizeof(int));
    if (!score || !used) { free(score); free(used); return 0; }

    for (int i = 0; i < n; ++i) {
        score[i] = cos_v126_score(cfg, q, &memories[i],
                                  m_sigmas ? &m_sigmas[i] : NULL);
    }
    int filled = 0;
    for (int j = 0; j < k; ++j) {
        int best = -1;
        float best_v = -2.0f;
        for (int i = 0; i < n; ++i) {
            if (used[i]) continue;
            if (score[i] > best_v) {
                best_v = score[i];
                best   = i;
            }
        }
        if (best < 0) break;
        used[best] = 1;
        out_idx[filled] = best;
        if (out_scores) out_scores[filled] = best_v;
        filled++;
    }
    free(score); free(used);
    return filled;
}

/* ====================================================================
 * Self-test
 * ================================================================== */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v126 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

static void sigma_uniform(cos_v126_sigma_profile_t *p, float v) {
    float ch[COS_V126_SIGMA_DIM];
    for (int i = 0; i < COS_V126_SIGMA_DIM; ++i) ch[i] = v;
    cos_v126_sigma_fill(p, ch);
}

int cos_v126_self_test(void) {
    cos_v126_config_t cfg;
    cos_v126_config_defaults(&cfg);

    /* --- Hidden projector ----------------------------------------- */
    fprintf(stderr, "check-v126: hidden-state projector L2-norm\n");
    float h[COS_V126_HIDDEN_DIM];
    cos_v126_hidden_from_text(
        "the quick brown fox jumps over the lazy dog", h);
    double n2 = 0.0;
    for (int i = 0; i < COS_V126_HIDDEN_DIM; ++i)
        n2 += (double)h[i] * (double)h[i];
    _CHECK(fabs(n2 - 1.0) < 1e-4, "hidden block is L2-unit");

    /* Empty text → zero vector (not normalized). */
    float h0[COS_V126_HIDDEN_DIM];
    cos_v126_hidden_from_text("", h0);
    double n0 = 0.0;
    for (int i = 0; i < COS_V126_HIDDEN_DIM; ++i)
        n0 += (double)h0[i] * (double)h0[i];
    _CHECK(n0 == 0.0, "empty text → zero vector");

    /* --- Identical content + identical σ → cosine ≈ 1 ------------- */
    fprintf(stderr, "check-v126: same-content + same-σ → cosine = 1\n");
    cos_v126_sigma_profile_t sA, sB;
    sigma_uniform(&sA, 0.30f);
    sigma_uniform(&sB, 0.30f);
    cos_v126_embedding_t eA, eB;
    cos_v126_embed(&cfg, "capital of finland is helsinki", &sA, &eA);
    cos_v126_embed(&cfg, "capital of finland is helsinki", &sB, &eB);
    float c_eq = cos_v126_cosine(&eA, &eB);
    _CHECK(c_eq > 0.9999f, "identical → cos=1");

    /* --- Identical content + divergent σ → cosine < 1 ------------- */
    fprintf(stderr, "check-v126: same-content + divergent-σ → cos < 1\n");
    cos_v126_sigma_profile_t sC;
    sigma_uniform(&sC, 0.90f);
    cos_v126_embedding_t eC;
    cos_v126_embed(&cfg, "capital of finland is helsinki", &sC, &eC);
    float c_div = cos_v126_cosine(&eA, &eC);
    _CHECK(c_div < 0.9999f,  "divergent σ drops similarity");
    _CHECK(c_div > 0.5f,     "content still dominates — cos stays > 0.5");

    /* --- Unrelated content + same σ → low cosine ------------------ */
    fprintf(stderr, "check-v126: unrelated content + same σ → cos << 1\n");
    cos_v126_embedding_t eD;
    cos_v126_embed(&cfg, "the hyperbolic tangent is the derivative of secant-h squared", &sA, &eD);
    float c_unrel = cos_v126_cosine(&eA, &eD);
    _CHECK(c_unrel < 0.4f,   "unrelated content → low cos");

    /* --- v115-style σ-weighted score ------------------------------ */
    fprintf(stderr, "check-v126: σ-weighted score down-ranks high-σ memory\n");
    /* Two memories with *the same content*, differing only in σ_product. */
    cos_v126_sigma_profile_t m_low, m_high;
    sigma_uniform(&m_low,  0.10f);
    sigma_uniform(&m_high, 0.85f);
    cos_v126_embedding_t e_q, e_mlow, e_mhigh;
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &sA,     &e_q);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &m_low,  &e_mlow);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &m_high, &e_mhigh);
    float s_low  = cos_v126_score(&cfg, &e_q, &e_mlow,  &m_low);
    float s_high = cos_v126_score(&cfg, &e_q, &e_mhigh, &m_high);
    _CHECK(s_low  > s_high,  "low-σ memory ranked above high-σ");
    _CHECK(s_low  > 0.80f,   "low-σ score still strong");
    _CHECK(s_high < s_low * 0.75f,
           "high-σ penalty substantive (≥25% drop)");

    /* --- Ranking: stable top-k -------------------------------------- */
    fprintf(stderr, "check-v126: rank_topk picks low-σ in-domain over high-σ\n");
    /* memories[0] on-topic with low σ; [1] on-topic high σ; [2] off-topic low σ */
    cos_v126_embedding_t mem[3];
    cos_v126_sigma_profile_t ms[3];
    sigma_uniform(&ms[0], 0.10f);
    sigma_uniform(&ms[1], 0.85f);
    sigma_uniform(&ms[2], 0.05f);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &ms[0], &mem[0]);
    cos_v126_embed(&cfg, "helsinki is the capital of finland", &ms[1], &mem[1]);
    cos_v126_embed(&cfg, "quantum chromodynamics confinement scale",
                   &ms[2], &mem[2]);
    int idx[3]; float sc[3];
    int nk = cos_v126_rank_topk(&cfg, &e_q, mem, ms, 3, idx, sc, 3);
    _CHECK(nk == 3,              "rank filled 3 slots");
    _CHECK(idx[0] == 0,          "top = in-domain low-σ");
    _CHECK(idx[1] == 1,          "second = in-domain high-σ (down-weighted)");
    _CHECK(idx[2] == 2,          "third = off-topic");
    _CHECK(sc[0] > sc[1],        "monotone scores");
    _CHECK(sc[1] > sc[2],        "monotone scores");

    /* --- σ_product from sigma_fill is geomean ---------------------- */
    fprintf(stderr, "check-v126: σ_product = geometric mean\n");
    float ch[COS_V126_SIGMA_DIM] = {0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f,0.2f};
    cos_v126_sigma_profile_t p;
    cos_v126_sigma_fill(&p, ch);
    _CHECK(fabsf(p.sigma_product - 0.2f) < 1e-4f, "geomean of constant");

    float ch2[COS_V126_SIGMA_DIM] = {0.1f,0.4f,0.1f,0.4f,0.1f,0.4f,0.1f,0.4f};
    cos_v126_sigma_fill(&p, ch2);
    /* geomean(0.1,0.4,...) = sqrt(0.04) = 0.2 */
    _CHECK(fabsf(p.sigma_product - 0.2f) < 1e-3f, "geomean alternating");

    fprintf(stderr, "check-v126: OK (hidden + composite + cosine + σ-weighted rank)\n");
    return 0;
}

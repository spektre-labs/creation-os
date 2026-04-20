/*
 * σ-RAG — local retrieval-augmented generation.
 *
 * See rag.h for the kernel contract.
 *
 * The embedder is intentionally zero-dependency: hashed-ngram
 * bag-of-words into a unit-norm vector.  Keep the hash stable — a
 * change flips every test vector downstream.  Upgrading to a real
 * model goes through σ-Plugin so this kernel stays pure and
 * offline.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "rag.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Small helpers
 * ================================================================== */
static float rag_clip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static char *rag_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    if (n > COS_RAG_MAX_TEXT) n = COS_RAG_MAX_TEXT;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* ASCII-fold + tolower + collapse whitespace into a fresh buffer.
 * Non-alphanumerics become ' '.  Returns malloc'd string; caller
 * frees.  Never returns NULL for a non-NULL input unless OOM. */
static char *rag_normalise(const char *text) {
    if (!text) return NULL;
    size_t n = strlen(text);
    char *buf = (char *)malloc(n + 2);
    if (!buf) return NULL;
    size_t j = 0;
    int in_space = 1;  /* leading ws trim */
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        char ch;
        if (c >= 'A' && c <= 'Z') {
            ch = (char)(c + 32);
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            ch = (char)c;
        } else {
            ch = ' ';
        }
        if (ch == ' ') {
            if (!in_space) {
                buf[j++] = ' ';
                in_space = 1;
            }
        } else {
            buf[j++] = ch;
            in_space = 0;
        }
    }
    while (j > 0 && buf[j - 1] == ' ') j--;
    buf[j] = '\0';
    return buf;
}

/* 64-bit FNV-1a. */
static uint64_t rag_hash64(const char *s, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) {
        h ^= (unsigned char)s[i];
        h *= 1099511628211ull;
    }
    return h;
}

/* ==================================================================
 * Embedding
 * ================================================================== */
int cos_rag_embed(const char *text, float *out) {
    if (!out) return COS_RAG_ERR_ARG;
    for (int i = 0; i < COS_RAG_DIM; i++) out[i] = 0.0f;
    if (!text) return COS_RAG_OK;

    char *norm = rag_normalise(text);
    if (!norm) return COS_RAG_ERR_OOM;

    /* Tokenise on single spaces.  Add unigrams, bigrams, trigrams
     * so we get a little sequence information on top of
     * bag-of-words.  Each feature contributes a signed unit
     * (based on the top bit of a secondary hash) to a dimension
     * indexed by (hash mod D). */
    size_t toks_cap = 64;
    size_t toks_n = 0;
    char **toks = (char **)malloc(toks_cap * sizeof(*toks));
    size_t *lens = (size_t *)malloc(toks_cap * sizeof(*lens));
    if (!toks || !lens) {
        free(toks); free(lens); free(norm);
        return COS_RAG_ERR_OOM;
    }

    char *p = norm;
    while (*p) {
        char *q = p;
        while (*q && *q != ' ') q++;
        if (q != p) {
            if (toks_n == toks_cap) {
                size_t ncap = toks_cap * 2;
                char **nt = (char **)realloc(toks, ncap * sizeof(*toks));
                size_t *nl = (size_t *)realloc(lens, ncap * sizeof(*lens));
                if (!nt || !nl) {
                    free(nt ? nt : toks);
                    free(nl ? nl : lens);
                    free(norm);
                    return COS_RAG_ERR_OOM;
                }
                toks = nt; lens = nl;
                toks_cap = ncap;
            }
            toks[toks_n] = p;
            lens[toks_n] = (size_t)(q - p);
            toks_n++;
        }
        if (*q) { *q = '\0'; p = q + 1; } else p = q;
    }

    /* Accumulate features.  Use a 128-byte scratch for n-gram
     * concatenation. */
    char buf[256];
    for (size_t i = 0; i < toks_n; i++) {
        for (int gram = 1; gram <= 3; gram++) {
            if (i + (size_t)gram > toks_n) break;
            size_t need = 0;
            for (int k = 0; k < gram; k++) {
                need += lens[i + (size_t)k] + 1;
            }
            if (need >= sizeof buf) continue;
            size_t off = 0;
            for (int k = 0; k < gram; k++) {
                if (k > 0) buf[off++] = ' ';
                memcpy(buf + off, toks[i + (size_t)k], lens[i + (size_t)k]);
                off += lens[i + (size_t)k];
            }
            buf[off] = '\0';
            uint64_t h = rag_hash64(buf, off);
            /* Mix gram into the hash so unigram "neural" and first
             * bigram token don't collide. */
            h ^= (uint64_t)gram * 0x9E3779B97F4A7C15ull;
            int idx = (int)(h % (uint64_t)COS_RAG_DIM);
            float sign = (h & 0x8000000000000000ull) ? -1.0f : 1.0f;
            float weight = 1.0f / (float)gram;  /* unigrams strongest */
            out[idx] += sign * weight;
        }
    }

    free(toks);
    free(lens);
    free(norm);

    /* L2-normalise. */
    double s2 = 0.0;
    for (int i = 0; i < COS_RAG_DIM; i++) s2 += (double)out[i] * (double)out[i];
    if (s2 <= 0.0) return COS_RAG_OK;  /* empty text → zero vector */
    double inv = 1.0 / sqrt(s2);
    for (int i = 0; i < COS_RAG_DIM; i++) out[i] = (float)((double)out[i] * inv);
    return COS_RAG_OK;
}

float cos_rag_cosine(const float *a, const float *b) {
    if (!a || !b) return 0.0f;
    double s = 0.0;
    for (int i = 0; i < COS_RAG_DIM; i++) s += (double)a[i] * (double)b[i];
    if (s > 1.0) s = 1.0;
    if (s < -1.0) s = -1.0;
    return (float)s;
}

float cos_rag_sigma_retrieval(float cosine) {
    float raw = 1.0f - cosine;  /* cosine ∈ [-1, 1] → raw ∈ [0, 2] */
    return rag_clip01(raw);
}

/* ==================================================================
 * Lifecycle
 * ================================================================== */
int cos_rag_index_init(cos_rag_index_t *idx,
                       int chunk_words, int chunk_overlap,
                       float tau_rag) {
    if (!idx) return COS_RAG_ERR_ARG;
    if (chunk_words <= 0)   chunk_words   = COS_RAG_CHUNK_WORDS_DEFAULT;
    if (chunk_overlap < 0)  chunk_overlap = 0;
    if (chunk_overlap >= chunk_words) chunk_overlap = chunk_words - 1;
    if (!(tau_rag >= 0.0f && tau_rag <= 1.0f)) tau_rag = COS_RAG_TAU_DEFAULT;

    memset(idx, 0, sizeof *idx);
    idx->cap = 64;
    idx->chunks = (cos_rag_chunk_t *)calloc((size_t)idx->cap, sizeof *idx->chunks);
    if (!idx->chunks) {
        idx->cap = 0;
        return COS_RAG_ERR_OOM;
    }
    idx->chunk_words = chunk_words;
    idx->chunk_overlap = chunk_overlap;
    idx->tau_rag = tau_rag;
    return COS_RAG_OK;
}

void cos_rag_index_free(cos_rag_index_t *idx) {
    if (!idx) return;
    for (int i = 0; i < idx->n_chunks; i++) {
        free(idx->chunks[i].text);
        free(idx->chunks[i].source_file);
    }
    free(idx->chunks);
    memset(idx, 0, sizeof *idx);
}

static int rag_grow(cos_rag_index_t *idx) {
    if (idx->n_chunks < idx->cap) return COS_RAG_OK;
    if (idx->cap >= COS_RAG_MAX_CHUNKS) return COS_RAG_ERR_FULL;
    int ncap = idx->cap * 2;
    if (ncap > COS_RAG_MAX_CHUNKS) ncap = COS_RAG_MAX_CHUNKS;
    cos_rag_chunk_t *nc = (cos_rag_chunk_t *)realloc(
        idx->chunks, (size_t)ncap * sizeof *nc);
    if (!nc) return COS_RAG_ERR_OOM;
    memset(nc + idx->cap, 0, (size_t)(ncap - idx->cap) * sizeof *nc);
    idx->chunks = nc;
    idx->cap = ncap;
    return COS_RAG_OK;
}

/* ==================================================================
 * Ingest
 * ================================================================== */
int cos_rag_append_chunk(cos_rag_index_t *idx,
                         const char *text,
                         const char *source_file,
                         float sigma_at_index) {
    if (!idx || !text) return COS_RAG_ERR_ARG;
    int rc = rag_grow(idx);
    if (rc != COS_RAG_OK) return rc;
    cos_rag_chunk_t *c = &idx->chunks[idx->n_chunks];
    /* Store normalised text so the round-trip demos are stable. */
    char *norm = rag_normalise(text);
    if (!norm) return COS_RAG_ERR_OOM;
    c->text = norm;
    c->source_file = source_file ? rag_strdup(source_file) : NULL;
    if (source_file && !c->source_file) {
        free(norm);
        return COS_RAG_ERR_OOM;
    }
    c->chunk_id = idx->n_chunks;
    c->sigma_at_index = rag_clip01(sigma_at_index);
    c->insertion_order = idx->insertion_counter++;
    if (cos_rag_embed(c->text, c->embedding) != COS_RAG_OK) {
        free(c->text); c->text = NULL;
        free(c->source_file); c->source_file = NULL;
        return COS_RAG_ERR_OOM;
    }
    idx->n_chunks++;
    return COS_RAG_OK;
}

int cos_rag_append_text(cos_rag_index_t *idx,
                        const char *text,
                        const char *source_file) {
    if (!idx || !text) return COS_RAG_ERR_ARG;
    char *norm = rag_normalise(text);
    if (!norm) return COS_RAG_ERR_OOM;
    if (!*norm) { free(norm); return 0; }

    /* Enumerate word boundaries in `norm`. */
    size_t nlen = strlen(norm);
    size_t words_cap = 64, words_n = 0;
    size_t *starts = (size_t *)malloc(words_cap * sizeof *starts);
    size_t *ends   = (size_t *)malloc(words_cap * sizeof *ends);
    if (!starts || !ends) {
        free(starts); free(ends); free(norm);
        return COS_RAG_ERR_OOM;
    }
    size_t i = 0;
    while (i < nlen) {
        while (i < nlen && norm[i] == ' ') i++;
        if (i >= nlen) break;
        size_t s = i;
        while (i < nlen && norm[i] != ' ') i++;
        if (words_n == words_cap) {
            size_t ncap = words_cap * 2;
            size_t *ns = (size_t *)realloc(starts, ncap * sizeof *ns);
            size_t *ne = (size_t *)realloc(ends,   ncap * sizeof *ne);
            if (!ns || !ne) {
                free(ns ? ns : starts);
                free(ne ? ne : ends);
                free(norm);
                return COS_RAG_ERR_OOM;
            }
            starts = ns; ends = ne; words_cap = ncap;
        }
        starts[words_n] = s;
        ends[words_n]   = i;
        words_n++;
    }

    if (words_n == 0) {
        free(starts); free(ends); free(norm);
        return 0;
    }

    int cw = idx->chunk_words;
    int co = idx->chunk_overlap;
    int step = cw - co;
    if (step <= 0) step = 1;

    int appended = 0;
    size_t pos = 0;
    char *buf = (char *)malloc(COS_RAG_MAX_TEXT + 1);
    if (!buf) {
        free(starts); free(ends); free(norm);
        return COS_RAG_ERR_OOM;
    }

    while (pos < words_n) {
        size_t end = pos + (size_t)cw;
        if (end > words_n) end = words_n;
        size_t boff = 0;
        for (size_t w = pos; w < end; w++) {
            size_t wlen = ends[w] - starts[w];
            if (w > pos) {
                if (boff + 1 >= (size_t)COS_RAG_MAX_TEXT) break;
                buf[boff++] = ' ';
            }
            if (boff + wlen >= (size_t)COS_RAG_MAX_TEXT) break;
            memcpy(buf + boff, norm + starts[w], wlen);
            boff += wlen;
        }
        buf[boff] = '\0';
        int rc = cos_rag_append_chunk(idx, buf, source_file, 0.0f);
        if (rc != COS_RAG_OK) {
            free(buf); free(starts); free(ends); free(norm);
            if (rc == COS_RAG_ERR_FULL && appended > 0) return appended;
            return rc;
        }
        appended++;
        if (end >= words_n) break;
        pos += (size_t)step;
    }

    free(buf); free(starts); free(ends); free(norm);
    return appended;
}

/* ==================================================================
 * Search
 * ================================================================== */
typedef struct {
    int   chunk_id;
    float score;
    uint64_t order;
} rag_cand_t;

static int rag_cmp_desc(const void *a, const void *b) {
    const rag_cand_t *x = (const rag_cand_t *)a;
    const rag_cand_t *y = (const rag_cand_t *)b;
    if (x->score > y->score) return -1;
    if (x->score < y->score) return  1;
    if (x->order < y->order) return -1;
    if (x->order > y->order) return  1;
    return 0;
}

int cos_rag_search(const cos_rag_index_t *idx,
                   const char *query,
                   int k,
                   int include_filtered,
                   cos_rag_hit_t *out,
                   int *out_n) {
    if (!idx || !query || !out || !out_n || k <= 0) return COS_RAG_ERR_ARG;
    *out_n = 0;
    if (idx->n_chunks == 0) return COS_RAG_OK;

    float qemb[COS_RAG_DIM];
    int rc = cos_rag_embed(query, qemb);
    if (rc != COS_RAG_OK) return rc;

    rag_cand_t *cands = (rag_cand_t *)malloc((size_t)idx->n_chunks * sizeof *cands);
    if (!cands) return COS_RAG_ERR_OOM;
    for (int i = 0; i < idx->n_chunks; i++) {
        cands[i].chunk_id = idx->chunks[i].chunk_id;
        cands[i].score    = cos_rag_cosine(qemb, idx->chunks[i].embedding);
        cands[i].order    = idx->chunks[i].insertion_order;
    }
    qsort(cands, (size_t)idx->n_chunks, sizeof *cands, rag_cmp_desc);

    int want = k < idx->n_chunks ? k : idx->n_chunks;
    int w = 0;
    for (int i = 0; i < idx->n_chunks && w < want; i++) {
        float sigma = cos_rag_sigma_retrieval(cands[i].score);
        if (!include_filtered && sigma > idx->tau_rag) continue;
        int cid = cands[i].chunk_id;
        out[w].chunk_id        = cid;
        out[w].score           = cands[i].score;
        out[w].sigma_retrieval = sigma;
        out[w].chunk           = &idx->chunks[cid];
        w++;
    }
    *out_n = w;
    free(cands);
    return COS_RAG_OK;
}

/* ==================================================================
 * Stats
 * ================================================================== */
void cos_rag_stats(const cos_rag_index_t *idx, cos_rag_stats_t *out) {
    if (!idx || !out) return;
    memset(out, 0, sizeof *out);
    out->n_chunks = idx->n_chunks;
    if (idx->n_chunks == 0) return;
    double ssum = 0.0, bsum = 0.0;
    int retained = 0;
    for (int i = 0; i < idx->n_chunks; i++) {
        ssum += idx->chunks[i].sigma_at_index;
        bsum += (double)strlen(idx->chunks[i].text);
        if (idx->chunks[i].sigma_at_index <= idx->tau_rag) retained++;
    }
    out->mean_sigma_at_index = (float)(ssum / idx->n_chunks);
    out->mean_text_bytes     = (float)(bsum / idx->n_chunks);
    out->n_retained          = retained;
}

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_rag_self_test(void) {
    /* 1. determinism of embedding */
    float a[COS_RAG_DIM], b[COS_RAG_DIM];
    cos_rag_embed("sigma pipeline rethink", a);
    cos_rag_embed("sigma pipeline rethink", b);
    for (int i = 0; i < COS_RAG_DIM; i++) if (a[i] != b[i]) return -1;

    /* 2. embedding is unit-norm for non-empty text */
    double s2 = 0.0;
    for (int i = 0; i < COS_RAG_DIM; i++) s2 += (double)a[i] * (double)a[i];
    if (s2 < 0.99 || s2 > 1.01) return -2;

    /* 3. empty text → zero vector → cosine 0 → σ_retrieval 1 */
    float z[COS_RAG_DIM];
    cos_rag_embed("", z);
    for (int i = 0; i < COS_RAG_DIM; i++) if (z[i] != 0.0f) return -3;

    /* 4. append + search smoke test */
    cos_rag_index_t idx;
    if (cos_rag_index_init(&idx, 8, 2, 0.9f) != COS_RAG_OK) return -4;
    const char *docs[] = {
        "sigma gate measures model confidence in creation os",
        "voice interface uses whisper and piper locally",
        "rag retrieves local chunks under sigma filter",
        "prometheus release adds continuous distillation",
    };
    for (int i = 0; i < 4; i++) {
        if (cos_rag_append_chunk(&idx, docs[i], "self_test", 0.1f) != COS_RAG_OK) {
            cos_rag_index_free(&idx);
            return -5;
        }
    }
    cos_rag_hit_t hits[4];
    int n = 0;
    if (cos_rag_search(&idx, "sigma gate confidence", 4, 1, hits, &n) != COS_RAG_OK) {
        cos_rag_index_free(&idx);
        return -6;
    }
    if (n < 1 || hits[0].chunk_id != 0) {
        cos_rag_index_free(&idx);
        return -7;
    }
    cos_rag_index_free(&idx);
    return 0;
}

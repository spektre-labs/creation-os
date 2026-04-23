/*
 * Semantic inference cache — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "inference_cache.h"

#include "../../core/cos_bsc.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach/mach_time.h>
#endif

#define COS_IC_MAGIC "COSICACH1"
#define COS_IC_VER   2u

static int64_t wall_ms(void)
{
#if defined(__APPLE__)
    static mach_timebase_info_data_t tb;
    if (tb.denom == 0)
        mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    uint64_t nano = t * (uint64_t)tb.numer / (uint64_t)tb.denom;
    return (int64_t)(nano / 1000000ULL);
#elif defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
    return (int64_t)time(NULL) * 1000LL;
#else
    return (int64_t)time(NULL) * 1000LL;
#endif
}

uint64_t cos_inference_prompt_fnv(const char *utf8)
{
    uint64_t h = 14695981039346656037ULL;
    if (!utf8)
        return h;
    for (const unsigned char *p = (const unsigned char *)utf8; *p; p++) {
        h ^= (uint64_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

float cos_inference_hamming_norm(const uint64_t *a, const uint64_t *b, int n_words)
{
    if (!a || !b || n_words <= 0 || n_words != COS_INF_W)
        return 1.f;
    uint32_t d = cos_hv_hamming(a, b);
    return (float)d / (float)COS_D;
}

static void hv_word(const char *word, uint64_t out[COS_INF_W])
{
    uint64_t seed = cos_inference_prompt_fnv(word);
    for (int i = 0; i < COS_INF_W; i++) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        out[i] = seed;
    }
}

static void xor_vec(uint64_t *dst, const uint64_t *a, const uint64_t *b)
{
    for (int i = 0; i < COS_INF_W; i++)
        dst[i] = a[i] ^ b[i];
}

static int split_words(const char *s, char words[][64], int maxw)
{
    int n = 0;
    const char *p = s ? s : "";
    while (*p && n < maxw) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        size_t w = 0;
        while (*p && !isspace((unsigned char)*p) && w + 1 < 64)
            words[n][w++] = *p++;
        words[n][w] = '\0';
        if (w > 0)
            n++;
    }
    return n;
}

void cos_inference_bsc_encode_prompt(const char *utf8, uint64_t out[COS_INF_W])
{
    char words[128][64];
    int nw = split_words(utf8, words, 128);
    memset(out, 0, (size_t)COS_INF_W * sizeof(uint64_t));
    if (nw <= 0)
        return;
    if (nw == 1) {
        hv_word(words[0], out);
        return;
    }
    uint64_t hv0[COS_INF_W], hv1[COS_INF_W], pair[COS_INF_W];
    for (int i = 0; i < nw - 1; i++) {
        hv_word(words[i], hv0);
        hv_word(words[i + 1], hv1);
        xor_vec(pair, hv0, hv1);
        for (int k = 0; k < COS_INF_W; k++)
            out[k] ^= pair[k];
    }
}

/* ---------------------- ring buffer ----------------------------- */

static struct cos_inference_cache_entry *g_ring;
static int                             g_cap;
static int                             g_head;
static int                             g_count;

static uint64_t g_stat_hits;
static uint64_t g_stat_misses;
static double   g_latency_saved_sum_ms;
static uint64_t g_latency_saved_n;

static float g_tau_strong = 0.35f;
static float g_tau_weak   = 0.45f;

static int            g_atexit_reg;

static void shutdown_save(void);

static int cache_path(char *buf, size_t cap)
{
    const char *env = getenv("COS_INFERENCE_CACHE_PATH");
    if (env && env[0])
        return snprintf(buf, cap, "%s", env) < (int)cap ? 0 : -1;
    const char *home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    return snprintf(buf, cap, "%s/.cos/inference_cache.bin", home) < (int)cap
               ? 0
               : -1;
}

int cos_inference_cache_init(int max_entries)
{
    if (max_entries <= 0)
        return -1;
    cos_inference_cache_clear(0);
    free(g_ring);
    g_ring = (struct cos_inference_cache_entry *)calloc(
        (size_t)max_entries, sizeof(struct cos_inference_cache_entry));
    if (!g_ring)
        return -1;
    g_cap   = max_entries;
    g_head  = 0;
    g_count = 0;
    const char *ts = getenv("COS_INFERENCE_TAU_STRONG");
    if (ts && ts[0])
        g_tau_strong = (float)atof(ts);
    ts = getenv("COS_INFERENCE_TAU_WEAK");
    if (ts && ts[0])
        g_tau_weak = (float)atof(ts);
    (void)cos_inference_cache_load();
    if (!g_atexit_reg) {
        g_atexit_reg = 1;
        atexit(shutdown_save);
    }
    return 0;
}

void cos_inference_cache_clear(int drop_file)
{
    if (g_ring && g_cap > 0)
        memset(g_ring, 0, (size_t)g_cap * sizeof(struct cos_inference_cache_entry));
    g_head = 0;
    g_count = 0;
    g_stat_hits = 0;
    g_stat_misses = 0;
    g_latency_saved_sum_ms = 0.0;
    g_latency_saved_n = 0;
    if (drop_file) {
        char path[512];
        if (cache_path(path, sizeof path) == 0)
            (void)remove(path);
    }
}

void cos_inference_cache_stats(int *total, int *hits, int *misses,
                               float *avg_latency_saved_ms)
{
    if (total)
        *total = g_count;
    if (hits)
        *hits = (int)g_stat_hits;
    if (misses)
        *misses = (int)g_stat_misses;
    if (avg_latency_saved_ms) {
        *avg_latency_saved_ms = (g_latency_saved_n > 0)
            ? (float)(g_latency_saved_sum_ms / (double)g_latency_saved_n)
            : 0.f;
    }
}

int cos_inference_cache_lookup(const uint64_t *bsc_prompt, int D_words,
                               struct cos_inference_cache_entry *hit_out)
{
    if (!bsc_prompt || !hit_out || D_words != COS_INF_W || !g_ring || g_cap <= 0)
        return 0;
    float best = 2.f;
    int best_i = -1;
    for (int i = 0; i < g_count; i++) {
        int idx = (g_head - g_count + g_cap + i) % g_cap;
        struct cos_inference_cache_entry *e = &g_ring[idx];
        if (e->response[0] == '\0')
            continue;
        float dist = cos_inference_hamming_norm(bsc_prompt, e->bsc_vector, COS_INF_W);
        if (dist < best) {
            best   = dist;
            best_i = idx;
        }
    }
    if (best_i < 0 || best >= g_tau_weak) {
        g_stat_misses++;
        return 0;
    }

    struct cos_inference_cache_entry *e = &g_ring[best_i];
    *hit_out = *e;
    e->hit_count++;
    hit_out->hit_count = e->hit_count;

    float bump = (best < g_tau_strong) ? 0.03f : 0.10f;
    hit_out->sigma = hit_out->sigma + bump;
    if (hit_out->sigma > 1.f)
        hit_out->sigma = 1.f;

    g_stat_hits++;
    if (e->latency_original_ms > 0)
        g_latency_saved_sum_ms += (double)e->latency_original_ms;
    g_latency_saved_n++;

    return 1;
}

int cos_inference_cache_peek_nearest(const uint64_t *bsc_prompt, int D_words,
                                     float *sigma_out, float *hamming_norm_out)
{
    if (!bsc_prompt || D_words != COS_INF_W || !g_ring || g_cap <= 0)
        return 0;
    float best = 2.f;
    int   best_i = -1;
    for (int i = 0; i < g_count; i++) {
        int idx = (g_head - g_count + g_cap + i) % g_cap;
        struct cos_inference_cache_entry *e = &g_ring[idx];
        if (e->response[0] == '\0')
            continue;
        float dist =
            cos_inference_hamming_norm(bsc_prompt, e->bsc_vector, COS_INF_W);
        if (dist < best) {
            best   = dist;
            best_i = idx;
        }
    }
    if (best_i < 0 || best >= g_tau_weak)
        return 0;
    if (sigma_out)
        *sigma_out = g_ring[best_i].sigma;
    if (hamming_norm_out)
        *hamming_norm_out = best;
    return 1;
}

int cos_inference_cache_store_latency(const uint64_t *bsc_prompt,
                                      const char *response, float sigma,
                                      int64_t latency_original_ms)
{
    if (!g_ring || g_cap <= 0 || !bsc_prompt || !response)
        return -1;
    struct cos_inference_cache_entry *slot =
        &g_ring[g_head % g_cap];
    memset(slot, 0, sizeof *slot);
    memcpy(slot->bsc_vector, bsc_prompt, sizeof(slot->bsc_vector));
    snprintf(slot->response, sizeof slot->response, "%s", response);
    slot->sigma                = sigma;
    slot->similarity_threshold = g_tau_strong;
    slot->hit_count            = 0;
    slot->timestamp_ms         = wall_ms();
    slot->latency_original_ms  = latency_original_ms;
    slot->prompt_hash          = 0;
    for (int i = 0; i < COS_INF_W; i++)
        slot->prompt_hash ^= slot->bsc_vector[i];

    g_head++;
    if (g_count < g_cap)
        g_count++;
    return 0;
}

int cos_inference_cache_store(const uint64_t *bsc_prompt, const char *response,
                              float sigma)
{
    return cos_inference_cache_store_latency(bsc_prompt, response, sigma, 0);
}

int cos_inference_cache_save(void)
{
    if (!g_ring || g_cap <= 0)
        return -1;
    char path[512];
    if (cache_path(path, sizeof path) != 0)
        return -1;
    char mkd[512];
    if (snprintf(mkd, sizeof mkd, "%s", path) > 0) {
        char *slash = strrchr(mkd, '/');
        if (slash && slash != mkd) {
            *slash = '\0';
            (void)mkdir(mkd, 0700);
        }
    }
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -2;
    fwrite(COS_IC_MAGIC, 1, strlen(COS_IC_MAGIC), fp);
    uint32_t ver = COS_IC_VER;
    fwrite(&ver, sizeof ver, 1, fp);
    fwrite(&g_cap, sizeof g_cap, 1, fp);
    fwrite(&g_head, sizeof g_head, 1, fp);
    fwrite(&g_count, sizeof g_count, 1, fp);
    fwrite(&g_stat_hits, sizeof g_stat_hits, 1, fp);
    fwrite(&g_stat_misses, sizeof g_stat_misses, 1, fp);
    fwrite(&g_latency_saved_sum_ms, sizeof g_latency_saved_sum_ms, 1, fp);
    fwrite(&g_latency_saved_n, sizeof g_latency_saved_n, 1, fp);
    size_t chunk = sizeof(struct cos_inference_cache_entry);
    if (fwrite(g_ring, chunk, (size_t)g_cap, fp) != (size_t)g_cap) {
        fclose(fp);
        return -3;
    }
    fclose(fp);
    return 0;
}

int cos_inference_cache_load(void)
{
    char path[512];
    if (cache_path(path, sizeof path) != 0)
        return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    char magic[16];
    size_t ml = strlen(COS_IC_MAGIC);
    if (fread(magic, 1, ml, fp) != ml || memcmp(magic, COS_IC_MAGIC, ml) != 0) {
        fclose(fp);
        return -1;
    }
    uint32_t ver = 0;
    if (fread(&ver, sizeof ver, 1, fp) != 1 || ver != COS_IC_VER) {
        fclose(fp);
        return -1;
    }
    int cap_r = 0, count_r = 0, head_r = 0;
    if (fread(&cap_r, sizeof cap_r, 1, fp) != 1
        || fread(&head_r, sizeof head_r, 1, fp) != 1
        || fread(&count_r, sizeof count_r, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    (void)fread(&g_stat_hits, sizeof g_stat_hits, 1, fp);
    (void)fread(&g_stat_misses, sizeof g_stat_misses, 1, fp);
    (void)fread(&g_latency_saved_sum_ms, sizeof g_latency_saved_sum_ms, 1, fp);
    (void)fread(&g_latency_saved_n, sizeof g_latency_saved_n, 1, fp);

    if (cap_r <= 0 || cap_r > 100000 || count_r < 0 || count_r > cap_r) {
        fclose(fp);
        return -1;
    }
    free(g_ring);
    g_ring = (struct cos_inference_cache_entry *)calloc(
        (size_t)cap_r, sizeof(struct cos_inference_cache_entry));
    if (!g_ring) {
        fclose(fp);
        return -2;
    }
    size_t chunk = sizeof(struct cos_inference_cache_entry);
    if (fread(g_ring, chunk, (size_t)cap_r, fp) != (size_t)cap_r) {
        fclose(fp);
        free(g_ring);
        g_ring = NULL;
        return -3;
    }
    fclose(fp);
    g_cap   = cap_r;
    g_head  = head_r;
    g_count = count_r;
    return 0;
}

void cos_inference_cache_dump_similar(const char *prompt_utf8, int max_lines)
{
    uint64_t q[COS_INF_W];
    cos_inference_bsc_encode_prompt(prompt_utf8, q);
    typedef struct {
        float dist;
        int   idx;
    } rank_t;
    rank_t rnk[256];
    int nr = 0;
    for (int i = 0; i < g_count && nr < 256; i++) {
        int idx = (g_head - g_count + g_cap + i) % g_cap;
        if (g_ring[idx].response[0] == '\0')
            continue;
        float d = cos_inference_hamming_norm(q, g_ring[idx].bsc_vector, COS_INF_W);
        rnk[nr].dist = d;
        rnk[nr].idx  = idx;
        nr++;
    }
    /* simple selection sort by dist */
    for (int i = 0; i < nr - 1; i++)
        for (int j = i + 1; j < nr; j++)
            if (rnk[j].dist < rnk[i].dist) {
                rank_t t = rnk[i];
                rnk[i]   = rnk[j];
                rnk[j]   = t;
            }
    int lim = max_lines > 0 ? max_lines : 8;
    if (lim > nr)
        lim = nr;
    for (int i = 0; i < lim; i++) {
        struct cos_inference_cache_entry *e = &g_ring[rnk[i].idx];
        fprintf(stdout, "  d=%.4f  σ=%.4f  hits=%d  ts=%lld  %s\n",
                (double)rnk[i].dist, (double)e->sigma, e->hit_count,
                (long long)e->timestamp_ms, e->response);
    }
}

static void shutdown_save(void)
{
    (void)cos_inference_cache_save();
}

int cos_inference_cache_self_test(void)
{
    char tmp[256];
    snprintf(tmp, sizeof tmp, "/tmp/cos_infer_cache_test_%d.bin",
             (int)getpid());
    setenv("COS_INFERENCE_CACHE_PATH", tmp, 1);

    cos_inference_cache_clear(1);
    if (cos_inference_cache_init(8) != 0)
        return 1;

    uint64_t a[COS_INF_W], b[COS_INF_W], z[COS_INF_W];
    cos_inference_bsc_encode_prompt(
        "stable semantic inference cache regression phrase alpha beta gamma", a);
    memcpy(b, a, sizeof(uint64_t) * (size_t)COS_INF_W);
    cos_inference_bsc_encode_prompt(
        "zz qq ww ee rr tt yy uu ii oo pp xx vv nn mm kk jj hh gg ff dd ss aa ll", z);
    float d_ident = cos_inference_hamming_norm(a, b, COS_INF_W);
    float d_far   = cos_inference_hamming_norm(a, z, COS_INF_W);
    if (d_ident > 1e-6f)
        return 2;
    if (!(d_far > d_ident))
        return 2;

    if (cos_inference_cache_store(a, "resp-a", 0.12f) != 0)
        return 3;
    float peek_sigma = 0.f, peek_d = 0.f;
    if (!cos_inference_cache_peek_nearest(a, COS_INF_W, &peek_sigma, &peek_d))
        return 3;
    if (peek_sigma < 0.119f || peek_sigma > 0.121f)
        return 3;
    struct cos_inference_cache_entry hit;
    if (cos_inference_cache_lookup(b, COS_INF_W, &hit) != 1)
        return 4;
    if (strstr(hit.response, "resp-a") == NULL)
        return 5;

    int tot = 0, hi = 0, mi = 0;
    float av = 0.f;
    cos_inference_cache_stats(&tot, &hi, &mi, &av);
    if (hi < 1)
        return 6;

    if (cos_inference_cache_save() != 0)
        return 7;
    cos_inference_cache_clear(0);
    if (cos_inference_cache_load() != 0)
        return 8;
    if (cos_inference_cache_lookup(b, COS_INF_W, &hit) != 1)
        return 9;

    cos_inference_cache_clear(1);
    remove(tmp);
    unsetenv("COS_INFERENCE_CACHE_PATH");
    return 0;
}

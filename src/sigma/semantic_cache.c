/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "semantic_cache.h"

#include "text_similarity.h"

#include <stdlib.h> /* getenv */
#include <string.h>
#include <time.h>

#define COS_SCACHE_N 256
#define COS_SCACHE_TTL 3600

typedef struct {
    uint64_t  prompt_hash;
    float     sigma_cached;
    char      response[4096];
    time_t    timestamp;
    uint64_t  touch; /* LRU: larger = more recent */
} sc_entry_t;

static sc_entry_t g_sc[COS_SCACHE_N];
static uint64_t   g_sc_touch_seq;

static int sc_disabled(void)
{
    const char *e = getenv("COS_SEMANTIC_LRU_CACHE");
    return (e != NULL && e[0] == '0' && e[1] == '\0');
}

uint64_t cos_semantic_cache_prompt_hash(const char *prompt)
{
    char        norm[2048];
    const unsigned char *p;
    uint64_t             h = 14695981039346656037ULL;

    if (prompt == NULL)
        return 0;
    cos_text_normalize(prompt, norm, (int)sizeof norm);
    p = (const unsigned char *)norm;
    for (; *p != '\0'; ++p) {
        h ^= (uint64_t)*p;
        h *= 1099511628211ULL;
    }
    return h ? h : 1ULL;
}

static int sc_find_slot(uint64_t hash, time_t now, int *empty_idx)
{
    int      best_evict = -1;
    uint64_t min_touch  = (uint64_t)(~0ULL);
    *empty_idx          = -1;

    for (int i = 0; i < COS_SCACHE_N; i++) {
        if (g_sc[i].prompt_hash == 0u) {
            if (*empty_idx < 0)
                *empty_idx = i;
            continue;
        }
        if (g_sc[i].prompt_hash == hash && g_sc[i].timestamp != 0
            && (now - g_sc[i].timestamp) < (time_t)COS_SCACHE_TTL)
            return i;
        if (g_sc[i].touch < min_touch) {
            min_touch  = g_sc[i].touch;
            best_evict = i;
        }
    }
    return (*empty_idx >= 0) ? -2 : best_evict;
}

float cos_semantic_cache_lookup(uint64_t hash, char *response_out,
                                size_t response_cap)
{
    if (sc_disabled() || hash == 0u)
        return -1.0f;

    time_t now = time(NULL);
    int    empty;
    int    i = sc_find_slot(hash, now, &empty);
    if (i < 0)
        return -1.0f;

    g_sc[i].touch = ++g_sc_touch_seq;
    if (response_out != NULL && response_cap > 0) {
        strncpy(response_out, g_sc[i].response, response_cap - 1u);
        response_out[response_cap - 1u] = '\0';
    }
    return g_sc[i].sigma_cached;
}

void cos_semantic_cache_store(uint64_t hash, float sigma, const char *response)
{
    if (sc_disabled() || hash == 0u)
        return;

    time_t now = time(NULL);
    int    empty;
    int    i = sc_find_slot(hash, now, &empty);
    if (i == -2)
        i = empty;
    else if (i < 0)
        i = empty >= 0 ? empty : 0;

    g_sc[i].prompt_hash  = hash;
    g_sc[i].sigma_cached = sigma;
    g_sc[i].timestamp    = now;
    g_sc[i].touch        = ++g_sc_touch_seq;
    if (response != NULL && response[0] != '\0') {
        strncpy(g_sc[i].response, response, sizeof g_sc[i].response - 1u);
        g_sc[i].response[sizeof g_sc[i].response - 1u] = '\0';
    } else {
        g_sc[i].response[0] = '\0';
    }
}

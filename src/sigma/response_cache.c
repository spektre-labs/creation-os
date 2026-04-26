/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "response_cache.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define COS_RCACHE_SIZE 256
#define COS_RCACHE_TTL_SEC 3600

typedef struct {
    uint64_t prompt_hash;
    float    sigma_cached;
    char     response[512];
    time_t   timestamp;
} cache_entry_t;

static cache_entry_t g_rcache[COS_RCACHE_SIZE];

uint64_t cos_response_cache_prompt_hash(const char *prompt)
{
    const unsigned char *p = (const unsigned char *)prompt;
    uint64_t             h = 14695981039346656037ULL;
    if (p == NULL)
        return 0;
    for (; *p != '\0'; ++p) {
        h ^= (uint64_t)*p;
        h *= 1099511628211ULL;
    }
    return h;
}

float cos_response_cache_lookup(uint64_t hash, char *response_out, int max_len)
{
    const char *dis = getenv("COS_RESPONSE_SIGMA_CACHE");
    if (dis != NULL && dis[0] == '0')
        return -1.0f;

    size_t       idx = (size_t)(hash % (uint64_t)COS_RCACHE_SIZE);
    cache_entry_t *e = &g_rcache[idx];
    time_t         now = time(NULL);
    if (e->prompt_hash == hash && e->timestamp != 0
        && (now - e->timestamp) < (time_t)COS_RCACHE_TTL_SEC) {
        if (response_out != NULL && max_len > 0) {
            strncpy(response_out, e->response, (size_t)max_len - 1);
            response_out[max_len - 1] = '\0';
        }
        return e->sigma_cached;
    }
    return -1.0f;
}

void cos_response_cache_store(uint64_t hash, float sigma, const char *response)
{
    const char *dis = getenv("COS_RESPONSE_SIGMA_CACHE");
    if (dis != NULL && dis[0] == '0')
        return;

    size_t       idx = (size_t)(hash % (uint64_t)COS_RCACHE_SIZE);
    cache_entry_t *e = &g_rcache[idx];
    e->prompt_hash   = hash;
    e->sigma_cached  = sigma;
    e->timestamp     = time(NULL);
    if (response != NULL && response[0] != '\0') {
        strncpy(e->response, response, sizeof e->response - 1u);
        e->response[sizeof e->response - 1u] = '\0';
    } else {
        e->response[0] = '\0';
    }
}

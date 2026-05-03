/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "cos_sigma_mirror.h"
#include "semantic_cache_bsc.h"
#include <string.h>

static uint32_t popcnt64(uint64_t x)
{
    uint32_t n = 0;
    while (x) {
        x &= x - 1u;
        n++;
    }
    return n;
}

void cos_semantic_cache_init(cos_semantic_cache_t *c)
{
    if (!c)
        return;
    memset(c, 0, sizeof *c);
}

int32_t cos_semantic_bsc_hamming(const uint64_t *a, const uint64_t *b,
                                int32_t n_words)
{
    int32_t i;
    int32_t h = 0;
    if (!a || !b || n_words <= 0)
        return 0;
    for (i = 0; i < n_words; i++)
        h += (int32_t)popcnt64(a[i] ^ b[i]);
    return h;
}

int32_t cos_semantic_bsc_similarity_q16(const uint64_t *a, const uint64_t *b,
                                       int32_t n_words)
{
    int32_t h;
    int64_t sim;
    int64_t denom;
    if (!a || !b)
        return 0;
    h = cos_semantic_bsc_hamming(a, b, n_words);
    denom = (int64_t)COS_SEM_BSC_BITS;
    if (denom <= 0)
        return 0;
    sim = ((int64_t)COS_SIGMA_Q16_ONE - 1)
        - (((int64_t)h * COS_SIGMA_Q16_ONE) / denom);
    if (sim < 0)
        sim = 0;
    if (sim > COS_SIGMA_Q16_ONE - 1)
        sim = COS_SIGMA_Q16_ONE - 1;
    return (int32_t)sim;
}

int cos_semantic_cache_lookup(cos_semantic_cache_t *c, const uint64_t *prompt_hash,
                             int32_t similarity_threshold_q16, int *out_idx)
{
    int32_t i;
    if (!c || !prompt_hash)
        return -1;
    for (i = 0; i < c->count; i++) {
        int32_t sim = cos_semantic_bsc_similarity_q16(
            prompt_hash, c->entries[i].prompt_hash, COS_SEM_BSC_WORDS);
        if (sim >= similarity_threshold_q16) {
            c->hits++;
            if (out_idx)
                *out_idx = (int)i;
            return 0;
        }
    }
    c->misses++;
    return -1;
}

void cos_semantic_cache_insert(cos_semantic_cache_t *c,
                              const cos_semantic_cache_entry_t *e)
{
    int32_t slot;
    if (!c || !e)
        return;
    if (c->count < COS_SEM_CACHE_ENTRIES) {
        slot = c->count;
        c->count++;
    } else {
        slot = c->cursor % COS_SEM_CACHE_ENTRIES;
        c->cursor++;
    }
    c->entries[slot] = *e;
}

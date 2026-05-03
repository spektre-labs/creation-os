/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Small integer semantic cache keyed by 4096-bit bipolar-style buckets
 * (BSC-friendly: Hamming / XOR popcount, no float cosine).
 */
#ifndef SEMANTIC_CACHE_BSC_H
#define SEMANTIC_CACHE_BSC_H

#include "cos_kv_cache_limits.h"
#include <stdint.h>

typedef struct {
    uint64_t prompt_hash[COS_SEM_BSC_WORDS];
    int32_t  sigma_q16;
    int32_t  verdict;
    int32_t  response_token;
} cos_semantic_cache_entry_t;

typedef struct {
    cos_semantic_cache_entry_t entries[COS_SEM_CACHE_ENTRIES];
    int32_t                    count;
    int32_t                    cursor;
    int32_t                    hits;
    int32_t                    misses;
} cos_semantic_cache_t;

void cos_semantic_cache_init(cos_semantic_cache_t *c);

int32_t cos_semantic_bsc_hamming(const uint64_t *a, const uint64_t *b,
                                int32_t n_words);

/* Similarity in Q16: 65535 − (hamming * 65536 / COS_SEM_BSC_BITS). */
int32_t cos_semantic_bsc_similarity_q16(const uint64_t *a, const uint64_t *b,
                                       int32_t n_words);

int cos_semantic_cache_lookup(cos_semantic_cache_t *c, const uint64_t *prompt_hash,
                             int32_t similarity_threshold_q16, int *out_idx);

void cos_semantic_cache_insert(cos_semantic_cache_t *c,
                              const cos_semantic_cache_entry_t *e);

#endif /* SEMANTIC_CACHE_BSC_H */

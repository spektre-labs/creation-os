/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * σ-guided KV cache: evict highest-interior σ first (protect endpoints).
 *
 * Storage layout: keys[l * max_len * head_dim + pos * head_dim + d]
 */
#ifndef SIGMA_KV_CACHE_H
#define SIGMA_KV_CACHE_H

#include <stdint.h>

typedef struct {
    int32_t *keys;
    int32_t *values;
    int32_t *token_sigmas; /* Q16.16 epistemic score per token slot */
    int32_t  cache_len;
    int32_t  max_len;
    int32_t  n_layers;
    int32_t  head_dim;
    int32_t  n_evicted;
    int32_t  n_tokens_seen; /* monotonic: increments on each committed token */
} sigma_kv_cache_t;

void sigma_kv_cache_init(sigma_kv_cache_t *c, int32_t *keys, int32_t *values,
                        int32_t *token_sigmas, int32_t max_len, int32_t n_layers,
                        int32_t head_dim);

int32_t sigma_kv_token_begin(sigma_kv_cache_t *c);
void sigma_kv_set_layer(sigma_kv_cache_t *c, int32_t layer, int32_t pos,
                       const int32_t *key, const int32_t *val);
void sigma_kv_token_commit(sigma_kv_cache_t *c, int32_t pos, int32_t sigma_q16);

void sigma_kv_evict_at(sigma_kv_cache_t *c, int32_t idx);
void sigma_kv_evict_worst_interior(sigma_kv_cache_t *c);

int32_t sigma_kv_prune_above(sigma_kv_cache_t *c, int32_t sigma_threshold_q16);

/* Q16.16 ratio cache_len / max_len (retention of physical slots). */
int32_t sigma_kv_slot_retention_q16(const sigma_kv_cache_t *c);

#endif /* SIGMA_KV_CACHE_H */

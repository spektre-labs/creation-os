/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* ACSL companion (clause ledger): hw/formal/v133/sigma_stack_contracts.acsl */
#include "sigma_kv_cache.h"
#include <stdint.h>
#include <string.h>

static int32_t row_bytes(const sigma_kv_cache_t *c)
{
    if (!c || c->head_dim <= 0)
        return 0;
    return c->head_dim * (int32_t)sizeof(int32_t);
}

void sigma_kv_cache_init(sigma_kv_cache_t *c, int32_t *keys, int32_t *values,
                        int32_t *token_sigmas, int32_t max_len, int32_t n_layers,
                        int32_t head_dim)
{
    if (!c)
        return;
    c->keys          = keys;
    c->values        = values;
    c->token_sigmas  = token_sigmas;
    c->cache_len     = 0;
    c->max_len       = max_len;
    c->n_layers      = n_layers;
    c->head_dim      = head_dim;
    c->n_evicted     = 0;
    c->n_tokens_seen = 0;
}

static void compact_layer_plane(int32_t *plane, int32_t max_len, int32_t head_dim,
                               int32_t cache_len, int32_t evict_idx)
{
    int32_t rb;
    int32_t tail_elems;
    int32_t base_src;
    int32_t base_dst;
    if (!plane || evict_idx < 0 || evict_idx >= cache_len - 1)
        return;
    rb         = head_dim * (int32_t)sizeof(int32_t);
    tail_elems = cache_len - 1 - evict_idx;
    base_dst   = evict_idx * head_dim;
    base_src   = (evict_idx + 1) * head_dim;
    if (tail_elems > 0)
        memmove(plane + base_dst, plane + base_src,
               (size_t)tail_elems * (size_t)rb);
    (void)max_len;
}

void sigma_kv_evict_at(sigma_kv_cache_t *c, int32_t idx)
{
    int32_t L;
    if (!c || !c->keys || !c->values || !c->token_sigmas)
        return;
    if (c->cache_len <= 0 || idx < 0 || idx >= c->cache_len)
        return;

    for (L = 0; L < c->n_layers; L++) {
        int32_t *kplane = c->keys + L * c->max_len * c->head_dim;
        int32_t *vplane = c->values + L * c->max_len * c->head_dim;
        compact_layer_plane(kplane, c->max_len, c->head_dim, c->cache_len, idx);
        compact_layer_plane(vplane, c->max_len, c->head_dim, c->cache_len, idx);
    }
    if (idx < c->cache_len - 1) {
        memmove(c->token_sigmas + idx, c->token_sigmas + idx + 1,
               (size_t)(c->cache_len - 1 - idx) * sizeof(int32_t));
    }
    c->cache_len--;
    c->n_evicted++;
}

void sigma_kv_evict_worst_interior(sigma_kv_cache_t *c)
{
    int32_t i;
    int32_t worst_idx;
    int32_t worst_sig;
    if (!c || c->cache_len < 3)
        return;
    worst_idx = 1;
    worst_sig = c->token_sigmas[1];
    for (i = 2; i < c->cache_len - 1; i++) {
        if (c->token_sigmas[i] > worst_sig) {
            worst_sig = c->token_sigmas[i];
            worst_idx = i;
        }
    }
    sigma_kv_evict_at(c, worst_idx);
}

static void ensure_room(sigma_kv_cache_t *c)
{
    while (c && c->cache_len >= c->max_len && c->max_len > 0) {
        if (c->cache_len >= 3)
            sigma_kv_evict_worst_interior(c);
        else if (c->cache_len > 0)
            sigma_kv_evict_at(c, 0);
        else
            break;
    }
}

int32_t sigma_kv_token_begin(sigma_kv_cache_t *c)
{
    if (!c)
        return -1;
    ensure_room(c);
    if (c->cache_len >= c->max_len)
        return -1;
    return c->cache_len;
}

void sigma_kv_set_layer(sigma_kv_cache_t *c, int32_t layer, int32_t pos,
                       const int32_t *key, const int32_t *val)
{
    int32_t off;
    int32_t rb;
    if (!c || !key || !val)
        return;
    if (layer < 0 || layer >= c->n_layers || pos < 0 || pos >= c->max_len)
        return;
    rb  = row_bytes(c);
    off = layer * c->max_len * c->head_dim + pos * c->head_dim;
    memcpy(c->keys + off, key, (size_t)rb);
    memcpy(c->values + off, val, (size_t)rb);
}

void sigma_kv_token_commit(sigma_kv_cache_t *c, int32_t pos, int32_t sigma_q16)
{
    if (!c || pos != c->cache_len)
        return;
    if (pos < 0 || pos >= c->max_len)
        return;
    c->token_sigmas[pos] = sigma_q16;
    c->cache_len++;
    c->n_tokens_seen++;
}

int32_t sigma_kv_prune_above(sigma_kv_cache_t *c, int32_t sigma_threshold_q16)
{
    int32_t pruned;
    int32_t i;
    if (!c || c->cache_len < 3)
        return 0;
    pruned = 0;
    /* Iterate backward so indices stay stable after each eviction. */
    for (i = c->cache_len - 2; i > 0; i--) {
        if (c->token_sigmas[i] > sigma_threshold_q16) {
            sigma_kv_evict_at(c, i);
            pruned++;
            if (c->cache_len < 3)
                break;
        }
    }
    return pruned;
}

int32_t sigma_kv_slot_retention_q16(const sigma_kv_cache_t *c)
{
    if (!c || c->max_len <= 0)
        return 0;
    return (int32_t)(((int64_t)c->cache_len * 65536) / (int64_t)c->max_len);
}

/*
 * v266 σ-Flash — FlashAttention + fused σ kernel.
 *
 *   FlashAttention keeps the Q·K^T softmax in SRAM —
 *   no HBM round-trip for the attention matrix.
 *   v266 fuses the σ computation into the same kernel
 *   so σ_attention = entropy(softmax(Q·K^T)) comes
 *   "free" (strict < 1 % overhead), drives KV-cache
 *   eviction (high-σ first), and prunes long-context
 *   tokens.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Attention heads (exactly 8, canonical indices 0..7):
 *     Each row: `index`, `sigma_head ∈ [0, 1]`,
 *     `overhead_pct ∈ [0.0, 1.0)` (σ fusion is strictly
 *     sub-1 % over raw FlashAttention), `fused == true`.
 *     Contract: every `fused`, every `overhead_pct < 1.0`.
 *
 *   Platform kernels (exactly 3, canonical order):
 *     cuda_sm90 · metal_m4 · neon_arm64
 *     Each row: `backend`, `supported == true`,
 *     `latency_ns > 0`, `sigma_fused == true`.
 *
 *   σ-aware KV cache (exactly 6 entries):
 *     Each row: `key`, `sigma_kv ∈ [0, 1]`,
 *     `evict_rank` (1 = evicted first).  Rule:
 *     evict_rank is the descending order of sigma_kv —
 *     high σ evicted first.  Contract: evict_rank is a
 *     permutation of [1..6] AND the sequence matches
 *     argsort(-sigma_kv).
 *
 *   Long-context pruning (exactly 2 scenarios: before,
 *   after):
 *     before: kept_tokens = 4 096, effective_ctx_k = 4
 *     after:  kept_tokens = 4 096, effective_ctx_k = 32
 *   Rule: same kept_tokens footprint (identical memory
 *   budget) AND after.effective_ctx_k > before.
 *   8× effective context at identical cache size.
 *
 *   σ_flash (surface hygiene):
 *       σ_flash = 1 −
 *         (heads_ok + platforms_ok + kv_rows_ok +
 *          kv_order_ok + pruning_ok) /
 *         (8 + 3 + 6 + 1 + 1)
 *   v0 requires `σ_flash == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 8 heads, all fused, all overhead_pct
 *        strictly < 1.0, σ_head ∈ [0, 1].
 *     2. Exactly 3 platforms in canonical order, all
 *        supported, latency_ns > 0, sigma_fused.
 *     3. Exactly 6 KV entries, σ_kv ∈ [0, 1],
 *        evict_rank permutation of [1..6] AND
 *        matches argsort(-sigma_kv).
 *     4. Pruning: same kept_tokens AND
 *        effective_ctx_k_after > effective_ctx_k_before
 *        AND pruning_ok.
 *     5. σ_flash ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v266.1 (named, not implemented): live CUDA /
 *     Metal / NEON kernels emitting per-head σ in the
 *     same pass, PagedAttention wired with σ_kv-driven
 *     eviction, σ-pruning driven by live v226
 *     attention feed over a real long-context run.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V266_FLASH_H
#define COS_V266_FLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V266_N_HEADS      8
#define COS_V266_N_PLATFORMS  3
#define COS_V266_N_KV         6

typedef struct {
    int    index;
    float  sigma_head;
    float  overhead_pct;
    bool   fused;
} cos_v266_head_t;

typedef struct {
    char    backend[16];
    bool    supported;
    int     latency_ns;
    bool    sigma_fused;
} cos_v266_platform_t;

typedef struct {
    char    key[16];
    float   sigma_kv;
    int     evict_rank;  /* 1 == first evicted */
} cos_v266_kv_t;

typedef struct {
    int  kept_tokens_before;
    int  kept_tokens_after;
    int  effective_ctx_k_before;
    int  effective_ctx_k_after;
    bool pruning_ok;
} cos_v266_pruning_t;

typedef struct {
    cos_v266_head_t      heads     [COS_V266_N_HEADS];
    cos_v266_platform_t  platforms [COS_V266_N_PLATFORMS];
    cos_v266_kv_t        kv        [COS_V266_N_KV];
    cos_v266_pruning_t   pruning;

    int   n_heads_ok;
    int   n_platforms_ok;
    int   n_kv_ok;
    bool  kv_order_ok;

    float sigma_flash;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v266_state_t;

void   cos_v266_init(cos_v266_state_t *s, uint64_t seed);
void   cos_v266_run (cos_v266_state_t *s);

size_t cos_v266_to_json(const cos_v266_state_t *s,
                         char *buf, size_t cap);

int    cos_v266_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V266_FLASH_H */

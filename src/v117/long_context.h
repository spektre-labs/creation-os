/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v117 σ-Long-Context — paged KV-cache manager with σ-aware eviction
 * and memory-offload hook.
 *
 * This kernel exports a *metadata* layer: it does not own the KV
 * tensors themselves (those live inside llama.cpp via the v101
 * bridge).  What it owns is:
 *
 *   - the page table (256-token pages, configurable);
 *   - a sliding window over the most recent N tokens that are kept
 *     with full attention;
 *   - σ-scores per page, fed back from the v101 bridge after each
 *     decode so we know which pages the model was most uncertain
 *     about;
 *   - the eviction policy (σ-aware LRU: highest-σ pages evicted
 *     first, ties broken by least-recently-used);
 *   - an offload callback that hands an evicted page's content to
 *     v115 σ-Memory so long-range context becomes persistent rather
 *     than lost.
 *
 * Why a separate kernel: llama.cpp's own KV cache grows linearly; a
 * 32k effective context on a 4k-native model requires the caller to
 * decide what to drop.  v117 makes that decision σ-explainable ("we
 * kept the low-σ reasoning chain, we dropped the high-σ tangent and
 * moved its summary to episodic memory").  No other long-context
 * system does σ-aware eviction today.
 *
 * v117.0 is the manager + policies + tests.  The llama.cpp plumb-in
 * (a hook in cos_v101_bridge_generate) lands in v117.1 and is gated
 * behind a compile-time flag; the HTTP surface advertises the
 * effective context budget via /v1/models.
 *
 * Zero dependencies beyond libc.  Thread-unsafe by design: callers
 * hold the v101 bridge lock across decode steps anyway.
 */
#ifndef COS_V117_LONG_CONTEXT_H
#define COS_V117_LONG_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V117_PAGE_TOKENS_DEFAULT   256
#define COS_V117_NATIVE_CTX_DEFAULT    4096
#define COS_V117_TARGET_CTX_DEFAULT    32768
#define COS_V117_SLIDING_TOKENS_DEFAULT 2048
#define COS_V117_MAX_PAGES              512

typedef enum {
    COS_V117_EVICT_LRU         = 0,
    COS_V117_EVICT_SIGMA_LRU   = 1,   /* default: σ first, LRU tiebreak */
    COS_V117_EVICT_SIGMA_ONLY  = 2,
} cos_v117_policy_t;

typedef enum {
    COS_V117_PAGE_FREE     = 0,
    COS_V117_PAGE_ACTIVE   = 1,       /* currently in KV cache */
    COS_V117_PAGE_EVICTED  = 2,       /* summarised into offload */
} cos_v117_page_state_t;

typedef struct cos_v117_page {
    int32_t id;                       /* stable id; -1 when free */
    int32_t token_start;              /* first token index covered */
    int32_t token_count;              /* actual tokens in this page */
    float   sigma_product;            /* last σ_product observed on page */
    int64_t last_used_step;           /* monotonic counter for LRU */
    cos_v117_page_state_t state;
    /* Optional user-attached summary / offload cookie (v115 row id). */
    int64_t offload_rowid;
    /* Content excerpt kept for summary / debugging (not the full KV). */
    char    preview[128];
} cos_v117_page_t;

/* Offload hook: invoked when a page is evicted.  Implementations can
 * write to v115 σ-Memory, to a file, or anywhere; returning a non-zero
 * cookie lets the manager reference the summary later.  `ud` is the
 * user-data pointer passed to `cos_v117_ctx_init`. */
typedef int64_t (*cos_v117_offload_fn)(const cos_v117_page_t *page,
                                       void *ud);

typedef struct cos_v117_config {
    int   page_tokens;          /* tokens per page; default 256 */
    int   native_ctx;           /* model's native context (e.g. 4096) */
    int   target_ctx;           /* effective context budget (e.g. 32768) */
    int   sliding_tokens;       /* recent-window kept with full attention */
    int   max_pages;            /* upper bound on page-table entries */
    cos_v117_policy_t policy;
} cos_v117_config_t;

typedef struct cos_v117_ctx cos_v117_ctx_t;

typedef struct cos_v117_stats {
    int tokens_ingested;        /* total tokens fed through ingest() */
    int tokens_active;          /* sum(token_count) over ACTIVE pages */
    int pages_active;
    int pages_evicted;
    int evictions_total;
    int offloads_total;
    int sliding_covered;        /* tokens actually in sliding window */
    float last_sigma_product;
} cos_v117_stats_t;

/* Fill a defaults config. */
void cos_v117_config_defaults(cos_v117_config_t *cfg);

/* Create / destroy a context.  `offload` may be NULL (eviction still
 * works; page content is simply discarded). */
cos_v117_ctx_t *cos_v117_ctx_init (const cos_v117_config_t *cfg,
                                    cos_v117_offload_fn offload, void *ud);
void             cos_v117_ctx_free(cos_v117_ctx_t *c);

/* Ingest a new token chunk.  `text` is copied into the current
 * page's preview buffer; `sigma_product` is the σ observed on this
 * chunk's generation (or 0.f for pre-filled prompt tokens).
 *
 * When the active-page budget exceeds `native_ctx`, eviction fires
 * according to the configured policy; evicted pages are offloaded.
 *
 * Returns number of pages evicted on this call (>= 0), or < 0 on
 * error. */
int cos_v117_ingest(cos_v117_ctx_t *c, const char *text, int token_count,
                    float sigma_product);

/* Explicit σ update for a page (e.g. when a later introspection pass
 * re-evaluates an old waypoint).  Returns 0 on success. */
int cos_v117_update_page_sigma(cos_v117_ctx_t *c, int32_t page_id,
                                float sigma_product);

/* Eviction / offload trigger (normally automatic).  `n` is the max
 * number of pages to evict.  Returns number actually evicted. */
int cos_v117_evict(cos_v117_ctx_t *c, int n);

/* Read-only views. */
const cos_v117_page_t *cos_v117_pages(const cos_v117_ctx_t *c, int *n_out);
void                    cos_v117_stats(const cos_v117_ctx_t *c,
                                        cos_v117_stats_t *out);

/* Serialize a {stats, pages[]} JSON view (for the HTTP /v1/context
 * endpoint).  Returns bytes written or -1 on overflow. */
int cos_v117_to_json(const cos_v117_ctx_t *c, char *out, size_t cap);

/* Pure-C self-test.  Covers:
 *  - default-config sanity
 *  - ingest/overflow/eviction under σ-LRU policy
 *  - σ_only policy evicts highest-σ even if MRU
 *  - sliding window stays covered
 *  - offload hook is invoked with the expected page metadata
 *  - JSON shape carries stats + pages
 * Returns 0 on pass. */
int cos_v117_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V117_LONG_CONTEXT_H */

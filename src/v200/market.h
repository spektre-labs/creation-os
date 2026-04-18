/*
 * v200 σ-Market — resource ledger, σ-price signal,
 * scarcity penalty, exchange receipts, and self-improving
 * cost.
 *
 *   Four resources are tracked on a deterministic ledger:
 *     R_COMPUTE        — v189 TTC budget (cpu-seconds)
 *     R_API            — calls to a larger external model
 *     R_MEMORY         — v115 SQLite bytes
 *     R_ADAPTERS       — v145 LoRA slots
 *
 *   σ is the price signal.  For every query:
 *     price = σ_query              (cheap when confident,
 *                                   expensive when uncertain)
 *   plus a per-resource scarcity penalty when a single
 *   allocator holds > τ_hoard of the pool (default 20 %):
 *     price *= (1 + penalty)
 *
 *   Exchange receipt: every ledger update logs
 *   (resource, amount, price, σ_before, σ_after) so ROI is
 *   falsifiable post-hoc.
 *
 *   Self-improving cost (v0 closed-form): every API call
 *   distils into a local skill.  The *next* time the same
 *   query arrives, σ drops and the route is local — so
 *   cost-per-query falls monotonically over a 40-query
 *   trajectory.
 *
 *   Contracts (v0):
 *     1. All 4 resources tracked; every exchange receipted.
 *     2. σ_query > σ_local ⇒ route = API;
 *        σ_query ≤ σ_local ⇒ route = local;
 *        mass-balance on the ledger holds.
 *     3. Anti-hoarding: no single allocator holds >
 *        τ_hoard of any pool after eviction (v0: deterministic
 *        trigger that evicts the highest-σ entries).
 *     4. Cost-per-query strictly decreases from first half
 *        to second half of the trajectory (self-improving).
 *     5. Scarcity penalty activates on R_MEMORY during the
 *        fixture (at least one tick with penalty > 0).
 *     6. FNV-1a exchange log replays byte-identically.
 *     7. Byte-deterministic overall.
 *
 * v200.1 (named): wired to v189 TTC budget, v120 distill
 *   latency metrics, v115 SQLite live sizing, and v145 LoRA
 *   capacity probe.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V200_MARKET_H
#define COS_V200_MARKET_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V200_N_QUERIES    40
#define COS_V200_N_RESOURCES   4
#define COS_V200_N_ALLOCATORS  5
#define COS_V200_STR_MAX      24

typedef enum {
    COS_V200_R_COMPUTE   = 0,
    COS_V200_R_API       = 1,
    COS_V200_R_MEMORY    = 2,
    COS_V200_R_ADAPTERS  = 3
} cos_v200_resource_t;

typedef enum {
    COS_V200_ROUTE_LOCAL = 0,
    COS_V200_ROUTE_API   = 1
} cos_v200_route_t;

typedef struct {
    int      id;
    int      resource;                      /* primary resource used */
    int      allocator;                     /* 0..N-1 */
    int      route;                         /* local / api */
    float    sigma_before;
    float    sigma_after;
    float    price;                         /* σ-priced */
    float    scarcity_penalty;              /* ≥ 0 */
    float    total_cost;                    /* price * (1+penalty) */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v200_exchange_t;

typedef struct {
    int                n_queries;
    cos_v200_exchange_t queries[COS_V200_N_QUERIES];

    /* Pools & holdings. */
    float              pool[COS_V200_N_RESOURCES];
    float              hold[COS_V200_N_RESOURCES][COS_V200_N_ALLOCATORS];
    float              tau_hoard;            /* 0.20 */
    float              sigma_local;          /* route threshold */

    /* Derived. */
    int                n_api_routes;
    int                n_local_routes;
    int                n_evictions;
    float              cost_first_half;
    float              cost_second_half;
    bool               scarcity_triggered;

    bool               chain_valid;
    uint64_t           terminal_hash;

    uint64_t           seed;
} cos_v200_state_t;

void   cos_v200_init(cos_v200_state_t *s, uint64_t seed);
void   cos_v200_build(cos_v200_state_t *s);
void   cos_v200_run(cos_v200_state_t *s);

size_t cos_v200_to_json(const cos_v200_state_t *s,
                         char *buf, size_t cap);

int    cos_v200_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V200_MARKET_H */

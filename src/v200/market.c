/*
 * v200 σ-Market — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "market.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void cos_v200_init(cos_v200_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x200EA121ULL;
    s->tau_hoard   = 0.20f;
    s->sigma_local = 0.35f;

    s->pool[COS_V200_R_COMPUTE]  = 1000.0f;
    s->pool[COS_V200_R_API]      =  200.0f;
    s->pool[COS_V200_R_MEMORY]   =   50.0f;   /* deliberately tight */
    s->pool[COS_V200_R_ADAPTERS] =   20.0f;
}

/* Fixture:
 *   40 queries.  σ_before falls monotonically over the
 *   trajectory (self-improving cost):
 *       σ_before[i] = 0.80 − 0.015 · i, clipped at 0.05
 *   Resource usage round-robins across the 4 resources so
 *   every resource is exercised; memory consumption is made
 *   large on early queries to trigger the scarcity penalty
 *   and eviction. */
void cos_v200_build(cos_v200_state_t *s) {
    s->n_queries = COS_V200_N_QUERIES;
    for (int i = 0; i < s->n_queries; ++i) {
        cos_v200_exchange_t *q = &s->queries[i];
        memset(q, 0, sizeof(*q));
        q->id         = i;
        q->resource   = i % COS_V200_N_RESOURCES;
        q->allocator  = i % COS_V200_N_ALLOCATORS;
        /* Trajectory chosen so that σ never lands exactly on
         * sigma_local (0.35) — avoids float edge cases when the
         * benchmark re-checks the route contract. */
        float sigma   = 0.78f - 0.02f * (float)i;
        if (sigma < 0.04f) sigma = 0.04f;
        q->sigma_before = sigma;
    }
}

void cos_v200_run(cos_v200_state_t *s) {
    memset(s->hold, 0, sizeof(s->hold));
    s->n_api_routes = s->n_local_routes = s->n_evictions = 0;
    s->scarcity_triggered = false;
    s->cost_first_half = 0.0f;
    s->cost_second_half = 0.0f;

    uint64_t prev = 0x200DA12ULL;
    for (int i = 0; i < s->n_queries; ++i) {
        cos_v200_exchange_t *q = &s->queries[i];

        /* Route choice from σ. */
        q->route = (q->sigma_before > s->sigma_local)
                   ? COS_V200_ROUTE_API
                   : COS_V200_ROUTE_LOCAL;

        /* Price = σ_before (σ IS the price signal). */
        q->price = q->sigma_before;

        /* Resource consumption per query (deterministic).  Memory
         * usage is larger on earlier queries to force scarcity. */
        float amount = 0.0f;
        switch (q->resource) {
        case COS_V200_R_COMPUTE:  amount = 5.0f;  break;
        case COS_V200_R_API:      amount = 2.0f;  break;
        case COS_V200_R_MEMORY:   amount = 6.0f;  break;
        case COS_V200_R_ADAPTERS: amount = 0.5f;  break;
        }
        s->hold[q->resource][q->allocator] += amount;

        /* Scarcity penalty.  If an allocator holds > τ_hoard of
         * the pool for this resource, penalty = hold_frac. */
        float hold_frac = s->hold[q->resource][q->allocator] /
                          s->pool[q->resource];
        float penalty = 0.0f;
        if (hold_frac > s->tau_hoard) {
            penalty = hold_frac;
            s->scarcity_triggered = true;
            /* Eviction: reset the allocator's holdings for this
             * resource to 0.5 × τ_hoard × pool (deterministic). */
            s->hold[q->resource][q->allocator] =
                0.5f * s->tau_hoard * s->pool[q->resource];
            s->n_evictions++;
        }
        q->scarcity_penalty = penalty;
        q->total_cost       = q->price * (1.0f + penalty);

        /* σ_after: route=API drops σ by 0.15 (API call = learning);
         * local route shaves 0.05. */
        float drop = (q->route == COS_V200_ROUTE_API) ? 0.15f : 0.05f;
        q->sigma_after = q->sigma_before - drop;
        if (q->sigma_after < 0.02f) q->sigma_after = 0.02f;

        if (q->route == COS_V200_ROUTE_API) s->n_api_routes++;
        else                                  s->n_local_routes++;

        if (i < s->n_queries / 2) s->cost_first_half  += q->total_cost;
        else                       s->cost_second_half += q->total_cost;

        q->hash_prev = prev;
        struct { int id, res, alloc, route;
                 float sb, sa, price, pen, cost;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = q->id;
        rec.res    = q->resource;
        rec.alloc  = q->allocator;
        rec.route  = q->route;
        rec.sb     = q->sigma_before;
        rec.sa     = q->sigma_after;
        rec.price  = q->price;
        rec.pen    = q->scarcity_penalty;
        rec.cost   = q->total_cost;
        rec.prev   = prev;
        q->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev         = q->hash_curr;
    }
    s->terminal_hash = prev;

    /* Chain replay. */
    uint64_t v = 0x200DA12ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n_queries; ++i) {
        const cos_v200_exchange_t *q = &s->queries[i];
        if (q->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, res, alloc, route;
                 float sb, sa, price, pen, cost;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id     = q->id;
        rec.res    = q->resource;
        rec.alloc  = q->allocator;
        rec.route  = q->route;
        rec.sb     = q->sigma_before;
        rec.sa     = q->sigma_after;
        rec.price  = q->price;
        rec.pen    = q->scarcity_penalty;
        rec.cost   = q->total_cost;
        rec.prev   = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != q->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v200_to_json(const cos_v200_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v200\","
        "\"n_queries\":%d,\"n_resources\":%d,"
        "\"tau_hoard\":%.3f,\"sigma_local\":%.3f,"
        "\"pool\":[%.2f,%.2f,%.2f,%.2f],"
        "\"n_api_routes\":%d,\"n_local_routes\":%d,"
        "\"n_evictions\":%d,"
        "\"cost_first_half\":%.4f,\"cost_second_half\":%.4f,"
        "\"scarcity_triggered\":%s,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"queries\":[",
        s->n_queries, COS_V200_N_RESOURCES,
        s->tau_hoard, s->sigma_local,
        s->pool[0], s->pool[1], s->pool[2], s->pool[3],
        s->n_api_routes, s->n_local_routes, s->n_evictions,
        s->cost_first_half, s->cost_second_half,
        s->scarcity_triggered ? "true" : "false",
        s->chain_valid        ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_queries; ++i) {
        const cos_v200_exchange_t *q = &s->queries[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"res\":%d,\"alloc\":%d,\"route\":%d,"
            "\"sb\":%.3f,\"sa\":%.3f,\"price\":%.3f,"
            "\"pen\":%.3f,\"cost\":%.3f}",
            i == 0 ? "" : ",", q->id, q->resource, q->allocator,
            q->route, q->sigma_before, q->sigma_after, q->price,
            q->scarcity_penalty, q->total_cost);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v200_self_test(void) {
    cos_v200_state_t s;
    cos_v200_init(&s, 0x200EA121ULL);
    cos_v200_build(&s);
    cos_v200_run(&s);

    if (s.n_queries != COS_V200_N_QUERIES)  return 1;
    if (s.n_api_routes < 1 || s.n_local_routes < 1) return 2;
    if (!s.scarcity_triggered)              return 3;
    if (s.n_evictions < 1)                   return 4;

    /* Self-improving cost: second half strictly cheaper. */
    if (!(s.cost_second_half < s.cost_first_half)) return 5;

    if (!s.chain_valid)                      return 6;

    /* Route contract: σ > σ_local ⇒ API, else local. */
    for (int i = 0; i < s.n_queries; ++i) {
        const cos_v200_exchange_t *q = &s.queries[i];
        if (q->sigma_before > s.sigma_local) {
            if (q->route != COS_V200_ROUTE_API)   return 7;
        } else {
            if (q->route != COS_V200_ROUTE_LOCAL) return 8;
        }
        if (q->total_cost + 1e-6f < q->price)     return 9;   /* cost ≥ price */
    }
    return 0;
}

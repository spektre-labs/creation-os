/*
 * v262 σ-Hybrid-Engine — multi-engine σ-routing.
 *
 *   Many engines, one σ-driven router.  BitNet (fast,
 *   small), AirLLM (slow, big), Engram (O(1) facts),
 *   cloud APIs (high-K) — all registered, all rated
 *   by σ, routed by difficulty, and billed.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Engine registry (exactly 5, canonical order):
 *     bitnet-3B-local     (tier=local_fast,  cost=0.00)
 *     airllm-70B-local    (tier=local_heavy, cost=0.00)
 *     engram-lookup       (tier=local_fact,  cost=0.00)
 *     api-claude          (tier=cloud,       cost>0)
 *     api-gpt             (tier=cloud,       cost>0)
 *   Each row: `name`, `tier`, `cost_per_1k_eur ≥ 0`,
 *   `sigma_floor ∈ [0, 1]` (the σ below which this
 *   engine is considered good enough).
 *
 *   σ-routing fixtures (exactly 4):
 *     Each fixture is a request with a *declared
 *     σ_difficulty* and the engine it MUST route to:
 *       difficulty ≤ 0.20  → bitnet-3B-local
 *       fact query         → engram-lookup
 *       difficulty ≤ 0.50  → airllm-70B-local
 *       difficulty >  0.50 → api-claude (or api-gpt)
 *     The v0 fixture requires ≥ 3 DISTINCT engines chosen
 *     across the 4 routing rows so the router has teeth.
 *
 *   Cascade (exactly 4 steps, canonical order):
 *     step 0: bitnet-3B-local,  σ_result = 0.60 → ESCALATE
 *     step 1: airllm-70B-local, σ_result = 0.30 → OK
 *     step 2: bitnet-3B-local,  σ_result = 0.10 → OK (no escalate)
 *     step 3: api-claude,       σ_result = 0.08 → OK
 *   Rule: `σ_result ≤ τ_accept (0.40)` → accepted (OK),
 *   else → ESCALATE.  v0 fixture:
 *     - step 0 MUST be ESCALATE (demonstrates cost-saving
 *       cascade)
 *     - at least one step is OK on the *first* engine
 *       (bitnet / engram) — proves 80 % path works
 *     - at least one step uses a cloud engine — proves
 *       fallback works
 *
 *   Cost tracking (monthly report):
 *     local_pct + api_pct == 100
 *     local_pct ≥ 80
 *     eur_sigma_route  (cost with σ routing)
 *     eur_api_only     (hypothetical cost if everything
 *                       went to cloud)
 *     savings_pct = round((eur_api_only − eur_sigma_route)
 *                         / eur_api_only × 100)
 *     savings_pct ≥ 80    (σ-routing saves ≥ 80 %)
 *
 *   σ_hybrid_engine (surface hygiene):
 *       σ_hybrid_engine = 1 −
 *         (engines_ok + routes_ok + distinct_engines_ok +
 *          cascade_ok + cost_ok) /
 *         (5 + 4 + 1 + 4 + 1)
 *   v0 requires `σ_hybrid_engine == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 5 engines in canonical order, every
 *        `sigma_floor ∈ [0, 1]`, cost ≥ 0, tier non-empty.
 *     2. Exactly 4 routing fixtures with chosen engine
 *        in the registry; ≥ 3 DISTINCT engines chosen.
 *     3. Exactly 4 cascade steps; decision matches σ vs
 *        τ_accept = 0.40; ≥ 1 ESCALATE AND ≥ 1 OK AND
 *        ≥ 1 cloud step.
 *     4. Cost report: local_pct + api_pct == 100,
 *        local_pct ≥ 80, savings_pct ≥ 80.
 *     5. σ_hybrid_engine ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v262.1 (named, not implemented): live cos engines
 *     CLI with auto-detection of running backends,
 *     real-time σ-router wired to v243 pipeline, actual
 *     invoice reconciliation for api-claude / api-gpt,
 *     σ-proxy cost optimiser as a cascade driver.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V262_HYBRID_ENGINE_H
#define COS_V262_HYBRID_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V262_N_ENGINES  5
#define COS_V262_N_ROUTES   4
#define COS_V262_N_CASCADE  4

typedef enum {
    COS_V262_CASCADE_OK       = 1,
    COS_V262_CASCADE_ESCALATE = 2
} cos_v262_cascade_dec_t;

typedef struct {
    char  name[24];
    char  tier[16];
    float cost_per_1k_eur;
    float sigma_floor;
    bool  engine_ok;
} cos_v262_engine_t;

typedef struct {
    char   request[24];
    float  sigma_difficulty;
    char   chosen_engine[24];
} cos_v262_route_t;

typedef struct {
    char                    engine[24];
    float                   sigma_result;
    cos_v262_cascade_dec_t  decision;
} cos_v262_cascade_t;

typedef struct {
    int   local_pct;
    int   api_pct;
    float eur_sigma_route;
    float eur_api_only;
    int   savings_pct;
    bool  cost_ok;
} cos_v262_cost_t;

typedef struct {
    cos_v262_engine_t   engines [COS_V262_N_ENGINES];
    cos_v262_route_t    routes  [COS_V262_N_ROUTES];
    cos_v262_cascade_t  cascade [COS_V262_N_CASCADE];
    cos_v262_cost_t     cost;

    float tau_accept;

    int   n_engines_ok;
    int   n_routes_ok;
    int   n_distinct_engines;
    int   n_cascade_ok;
    int   n_escalate;
    int   n_cloud_used;

    float sigma_hybrid_engine;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v262_state_t;

void   cos_v262_init(cos_v262_state_t *s, uint64_t seed);
void   cos_v262_run (cos_v262_state_t *s);

size_t cos_v262_to_json(const cos_v262_state_t *s,
                         char *buf, size_t cap);

int    cos_v262_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V262_HYBRID_ENGINE_H */

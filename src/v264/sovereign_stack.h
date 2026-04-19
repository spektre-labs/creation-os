/*
 * v264 σ-Sovereign-Stack — full sovereign pino.
 *
 *   One stack, zero cloud dependency.  v264 encodes
 *   "any-machine hardware → BitNet + AirLLM (local) →
 *   Engram + SQLite (memory) → σ-gate → mesh (P2P) →
 *   optional API fallback" as a typed manifest, plus an
 *   offline-mode predicate, sovereign-identity anchor
 *   set, and a strict cost-reduction claim (~200 €/mo →
 *   ~20 €/mo).  "Its like a coffee bro."
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Stack layers (exactly 7, canonical order):
 *     0: hardware       — cpu+gpu; min_gpu_gb ≥ 4
 *     1: model          — bitnet-3B / airllm-70B (local)
 *     2: memory         — engram_o1 + sqlite_session
 *     3: gate           — sigma_gate (0.6 ns, branchless)
 *     4: network        — mesh (P2P, no central)
 *     5: api_fallback   — cloud (used only when σ forces)
 *     6: license        — AGPL + SCSL
 *   Each row: `name`, `open_source` (bool), `works_offline`
 *   (bool), `requires_cloud` (bool).
 *   Contracts:
 *     - every layer 0..5 is `open_source == true`
 *     - layers 0..4 AND 6 are `works_offline == true`
 *       (layer 5 is the cloud fallback; `works_offline`
 *       is false by definition)
 *     - only layer 5 has `requires_cloud == true`
 *
 *   Offline flows (exactly 4, canonical order):
 *     helper_query, explain_concept, fact_lookup,
 *     reasoning_chain.
 *   Each row: `flow`, `handled_by_engine` (MUST be a
 *   local engine: bitnet-3B-local, airllm-70B-local,
 *   engram-lookup), `used_cloud == false`, `ok == true`.
 *   Contract: every flow runs offline; cloud is never
 *   touched in this manifest; distinct local engines
 *   used ≥ 2 so offline has teeth.
 *
 *   Sovereign-identity anchors (exactly 4, canonical
 *   order): v234_presence · v182_privacy · v148_sovereign
 *   · v238_sovereignty. Each row: `kernel`, `role`,
 *   `fulfilled == true`.
 *
 *   Cost model (monthly € budget):
 *     eur_baseline         (reference: "hobby bro" = 200)
 *     eur_sigma_sovereign  (target:   "coffee bro" = 20)
 *     reduction_pct = round(100 × (baseline − sovereign)
 *                           / baseline)
 *   Contract: baseline == 200 AND sovereign == 20 AND
 *   reduction_pct == 90.
 *
 *   σ_sovereign_stack (surface hygiene):
 *       σ_sovereign_stack = 1 −
 *         (layers_ok + offline_flows_ok +
 *          distinct_local_engines_ok +
 *          anchors_ok + cost_ok) /
 *         (7 + 4 + 1 + 4 + 1)
 *   v0 requires `σ_sovereign_stack == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 7 stack layers in canonical order;
 *        every `open_source == true` for 0..5; only
 *        layer 5 has `requires_cloud == true`; layers
 *        0..4 and 6 have `works_offline == true`.
 *     2. Exactly 4 offline flows; every `used_cloud ==
 *        false`, `ok == true`; ≥ 2 distinct local
 *        engines used.
 *     3. Exactly 4 sovereign anchors fulfilled.
 *     4. Cost: baseline = 200, sovereign = 20,
 *        reduction_pct = 90.
 *     5. σ_sovereign_stack ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v264.1 (named, not implemented): live boot target
 *     `cos start --offline`, auto-detected hardware
 *     profile, wired mesh P2P stack, signed sovereign
 *     identity, real invoice reconciliation for the
 *     20 €/mo claim.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V264_SOVEREIGN_STACK_H
#define COS_V264_SOVEREIGN_STACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V264_N_LAYERS    7
#define COS_V264_N_FLOWS     4
#define COS_V264_N_ANCHORS   4

typedef struct {
    char  name[20];
    char  detail[28];
    bool  open_source;
    bool  works_offline;
    bool  requires_cloud;
} cos_v264_layer_t;

typedef struct {
    char  flow[24];
    char  engine[24];
    bool  used_cloud;
    bool  ok;
} cos_v264_flow_t;

typedef struct {
    char  kernel[16];
    char  role  [24];
    bool  fulfilled;
} cos_v264_anchor_t;

typedef struct {
    int   eur_baseline;
    int   eur_sigma_sovereign;
    int   reduction_pct;
    bool  cost_ok;
} cos_v264_cost_t;

typedef struct {
    cos_v264_layer_t   layers  [COS_V264_N_LAYERS];
    cos_v264_flow_t    flows   [COS_V264_N_FLOWS];
    cos_v264_anchor_t  anchors [COS_V264_N_ANCHORS];
    cos_v264_cost_t    cost;

    int   n_layers_ok;
    int   n_flows_ok;
    int   n_distinct_local_engines;
    int   n_anchors_ok;

    float sigma_sovereign_stack;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v264_state_t;

void   cos_v264_init(cos_v264_state_t *s, uint64_t seed);
void   cos_v264_run (cos_v264_state_t *s);

size_t cos_v264_to_json(const cos_v264_state_t *s,
                         char *buf, size_t cap);

int    cos_v264_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V264_SOVEREIGN_STACK_H */

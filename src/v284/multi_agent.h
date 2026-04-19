/*
 * v284 σ-Multi-Agent — framework-agnostic σ-layer for
 *                      agent orchestration.
 *
 *   LangGraph, CrewAI, AutoGen, and OpenAI Swarm all
 *   route agents, none of them measure σ.  Creation OS
 *   sits as middleware above every one of them: each
 *   adapter accepts an agent-decision event and gates
 *   it through σ_product.  v284 types four falsifiable
 *   manifests covering adapter coverage, agent-to-
 *   agent σ, σ-weighted consensus fusion, and
 *   cost-optimal σ-driven routing.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Framework adapters (exactly 4 canonical rows):
 *     `langgraph` · `crewai` · `autogen` · `swarm`,
 *     each `adapter_enabled == true` AND
 *     `sigma_middleware == true`.
 *     Contract: canonical names in order; 4 distinct;
 *     every adapter enabled AND σ-middleware live.
 *
 *   Agent-to-agent σ (exactly 4 fixtures,
 *   τ_a2a = 0.40):
 *     Each: `sender_id`, `receiver_id`,
 *     `σ_message ∈ [0, 1]`,
 *     `decision ∈ {TRUST, VERIFY}`,
 *     rule: `σ_message ≤ τ_a2a → TRUST` else VERIFY.
 *     Contract: ≥ 1 TRUST AND ≥ 1 VERIFY — the
 *     receiver trusts on low-σ inbound messages and
 *     re-verifies on high-σ ones, stopping failure
 *     cascades in the agent mesh.
 *
 *   σ-weighted consensus (exactly 5 agents):
 *     Each: `agent_id`, `σ_agent ∈ [0, 1]`, `weight`,
 *     `is_winner`.
 *     Weight rule: `weight_i = (1 − σ_agent_i) /
 *     Σ_j (1 − σ_agent_j)` so the Σ weights = 1.0
 *     (± 1e-3).
 *     Winner rule: `is_winner == true` iff
 *     `agent_id == argmin_i σ_agent_i` (equivalently
 *     `argmax_i weight_i`); exactly one winner.
 *     Contract: Σ weights = 1.0; argmin σ and argmax
 *     weight pick the same agent; exactly one
 *     `is_winner`.
 *
 *   Cost-optimal routing (exactly 4 canonical
 *   tiers, cascade τ_easy = 0.20, τ_medium = 0.50,
 *   τ_hard = 0.80):
 *     `easy` · `medium` · `hard` · `critical`,
 *     each: `label`, `σ_difficulty ∈ [0, 1]`,
 *     `n_agents`, `mode ∈ {LOCAL, NEGOTIATE,
 *     CONSENSUS, HITL}`.
 *     Cascade:
 *       σ_difficulty ≤ 0.20 → LOCAL      (1 agent)
 *       σ_difficulty ≤ 0.50 → NEGOTIATE  (2 agents)
 *       σ_difficulty ≤ 0.80 → CONSENSUS  (5 agents)
 *       else                → HITL       (0 agents,
 *                                         human in
 *                                         the loop)
 *     Contract: each tier fires exactly once
 *     (permutation of the four modes); n_agents
 *     matches the mode.
 *
 *   σ_multiagent (surface hygiene):
 *       σ_multiagent = 1 −
 *         (adapter_rows_ok + adapter_distinct_ok +
 *          a2a_rows_ok + a2a_both_ok +
 *          consensus_rows_ok + consensus_weights_ok +
 *          consensus_argmax_ok + routing_rows_ok +
 *          routing_distinct_ok) /
 *         (4 + 1 + 4 + 1 + 5 + 1 + 1 + 4 + 1)
 *   v0 requires `σ_multiagent == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 adapter rows canonical; all enabled; all
 *        distinct.
 *     2. 4 agent-to-agent fixtures; decision matches
 *        σ vs τ_a2a; TRUST and VERIFY both fire.
 *     3. 5 consensus rows; weights sum to 1.0 ± 1e-3;
 *        is_winner matches argmin σ AND argmax
 *        weight; exactly one winner.
 *     4. 4 routing rows canonical; cascade picks each
 *        mode exactly once; n_agents matches mode.
 *     5. σ_multiagent ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v284.1 (named, not implemented): live middleware
 *     adapters for LangGraph / CrewAI / AutoGen /
 *     Swarm with a typed hand-off event, measured
 *     σ_message on real agent traffic, and a
 *     production HITL escalation path on the
 *     `critical` tier.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V284_MULTI_AGENT_H
#define COS_V284_MULTI_AGENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V284_N_ADAPTER    4
#define COS_V284_N_A2A        4
#define COS_V284_N_CONSENSUS  5
#define COS_V284_N_ROUTING    4

typedef enum {
    COS_V284_A2A_TRUST  = 1,
    COS_V284_A2A_VERIFY = 2
} cos_v284_a2a_t;

typedef enum {
    COS_V284_MODE_LOCAL      = 1,
    COS_V284_MODE_NEGOTIATE  = 2,
    COS_V284_MODE_CONSENSUS  = 3,
    COS_V284_MODE_HITL       = 4
} cos_v284_mode_t;

typedef struct {
    char  name[12];
    bool  adapter_enabled;
    bool  sigma_middleware;
} cos_v284_adapter_t;

typedef struct {
    int             sender_id;
    int             receiver_id;
    float           sigma_message;
    cos_v284_a2a_t  decision;
} cos_v284_a2a_row_t;

typedef struct {
    int   agent_id;
    float sigma_agent;
    float weight;
    bool  is_winner;
} cos_v284_consensus_t;

typedef struct {
    char             label[10];
    float            sigma_difficulty;
    int              n_agents;
    cos_v284_mode_t  mode;
} cos_v284_routing_t;

typedef struct {
    cos_v284_adapter_t    adapter   [COS_V284_N_ADAPTER];
    cos_v284_a2a_row_t    a2a       [COS_V284_N_A2A];
    cos_v284_consensus_t  consensus [COS_V284_N_CONSENSUS];
    cos_v284_routing_t    routing   [COS_V284_N_ROUTING];

    float tau_a2a;           /* 0.40 */
    float tau_easy;          /* 0.20 */
    float tau_medium;        /* 0.50 */
    float tau_hard;          /* 0.80 */

    int   n_adapter_rows_ok;
    bool  adapter_distinct_ok;

    int   n_a2a_rows_ok;
    int   n_a2a_trust;
    int   n_a2a_verify;

    int   n_consensus_rows_ok;
    bool  consensus_weights_ok;
    bool  consensus_argmax_ok;
    int   winner_agent_id;

    int   n_routing_rows_ok;
    bool  routing_distinct_ok;
    int   n_mode_local;
    int   n_mode_negotiate;
    int   n_mode_consensus;
    int   n_mode_hitl;

    float sigma_multiagent;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v284_state_t;

void   cos_v284_init(cos_v284_state_t *s, uint64_t seed);
void   cos_v284_run (cos_v284_state_t *s);

size_t cos_v284_to_json(const cos_v284_state_t *s,
                         char *buf, size_t cap);

int    cos_v284_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V284_MULTI_AGENT_H */

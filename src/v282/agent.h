/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v282 σ-Agent — autonomous agent with σ on every
 *                action, chain, tool, and failure.
 *
 *   Agents (tool use, multi-step planning, long-horizon
 *   tasks) are the 2026 headline, and σ is what makes
 *   them trustworthy.  v282 types four merge-gate
 *   manifests:
 *     * per-action σ-gate (AUTO / ASK / BLOCK);
 *     * multi-step σ propagation (confidence product
 *       across a chain so a long, risky plan fails by
 *       construction);
 *     * tool-selection σ cascade (USE / SWAP / BLOCK);
 *     * failure recovery where σ strictly increases on
 *       the post-failure re-evaluation of the same
 *       context and the σ-gate update is applied.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Action gate (exactly 4 fixtures, τ_auto = 0.20,
 *   τ_ask = 0.60):
 *     Each: `action_id`, `σ_action ∈ [0, 1]`,
 *     `decision ∈ {AUTO, ASK, BLOCK}`,
 *     cascade:
 *       σ_action ≤ τ_auto → AUTO
 *       σ_action ≤ τ_ask  → ASK
 *       else              → BLOCK.
 *     Contract: every branch fires ≥ 1×.
 *
 *   Propagation (exactly 2 chains, canonical order
 *   `short`, `long`, τ_chain = 0.70):
 *     Each: `label`, `n_steps`, `sigma_per_step ∈
 *     [0, 1]`, `sigma_total ∈ [0, 1]`,
 *     `verdict ∈ {PROCEED, ABORT}`.
 *     Math: `sigma_total = 1 − (1 − sigma_per_step) ^
 *     n_steps`.
 *     Rule: `sigma_total ≤ τ_chain → PROCEED` else
 *     `ABORT`.
 *     Fixed fixtures:
 *       short (3 steps, σ_per_step = 0.10) →
 *         σ_total ≈ 0.271 → PROCEED
 *       long  (10 steps, σ_per_step = 0.30) →
 *         σ_total ≈ 0.972 → ABORT
 *     Contract: PROCEED and ABORT each fire exactly
 *     once AND the computed σ_total matches the
 *     expected value within 1e-4.
 *
 *   Tool selection (exactly 3 fixtures, canonical
 *   order `correct`, `wrong_light`, `wrong_heavy`,
 *   τ_tool_use = 0.30, τ_tool_swap = 0.60):
 *     Each: `tool_id`, `σ_tool ∈ [0, 1]`,
 *     `decision ∈ {USE, SWAP, BLOCK}`,
 *     cascade:
 *       σ_tool ≤ τ_tool_use  → USE
 *       σ_tool ≤ τ_tool_swap → SWAP
 *       else                 → BLOCK.
 *     Contract: every branch fires ≥ 1×.
 *
 *   Recovery (exactly 3 fixtures):
 *     Each: `context_id`, `sigma_first_try ∈ [0, 1]`,
 *     `sigma_after_fail ∈ [0, 1]`,
 *     `gate_update_applied`.
 *     Contract: `sigma_after_fail > sigma_first_try`
 *     strictly for every row AND
 *     `gate_update_applied == true` for every row —
 *     the agent has to learn from the failure and
 *     apply the σ-gate update.
 *
 *   σ_agent (surface hygiene):
 *       σ_agent = 1 −
 *         (action_rows_ok + action_all_branches_ok +
 *          prop_rows_ok + prop_math_ok + prop_both_ok +
 *          tool_rows_ok + tool_all_branches_ok +
 *          recovery_rows_ok + recovery_monotone_ok +
 *          recovery_updates_ok) /
 *         (4 + 1 + 2 + 1 + 1 + 3 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_agent == 0.0`.
 *
 *   Contracts (v0):
 *     1. 4 action rows; cascade fires all three
 *        branches.
 *     2. 2 propagation chains; math matches product
 *        formula; PROCEED and ABORT each fire exactly
 *        once.
 *     3. 3 tool rows canonical; cascade fires all
 *        three branches.
 *     4. 3 recovery rows with σ_after_fail strictly
 *        greater than σ_first_try AND gate update
 *        applied on every row.
 *     5. σ_agent ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v282.1 (named, not implemented): live agent
 *     runtime wired into v262 with σ-gated MCP tool
 *     calls, real multi-step plan propagation,
 *     production fail / re-evaluate loop hooked into
 *     v275 TTT weight updates, and a measured
 *     abstention curve (AURCC) on a human-preference
 *     benchmark.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V282_AGENT_H
#define COS_V282_AGENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V282_N_ACTION     4
#define COS_V282_N_CHAIN      2
#define COS_V282_N_TOOL       3
#define COS_V282_N_RECOVERY   3

typedef enum {
    COS_V282_ACT_AUTO  = 1,
    COS_V282_ACT_ASK   = 2,
    COS_V282_ACT_BLOCK = 3
} cos_v282_act_t;

typedef enum {
    COS_V282_VER_PROCEED = 1,
    COS_V282_VER_ABORT   = 2
} cos_v282_ver_t;

typedef enum {
    COS_V282_TOOL_USE   = 1,
    COS_V282_TOOL_SWAP  = 2,
    COS_V282_TOOL_BLOCK = 3
} cos_v282_tool_t;

typedef struct {
    int             action_id;
    float           sigma_action;
    cos_v282_act_t  decision;
} cos_v282_action_t;

typedef struct {
    char            label[8];
    int             n_steps;
    float           sigma_per_step;
    float           sigma_total;
    cos_v282_ver_t  verdict;
} cos_v282_chain_t;

typedef struct {
    char             tool_id[14];
    float            sigma_tool;
    cos_v282_tool_t  decision;
} cos_v282_tool_row_t;

typedef struct {
    int   context_id;
    float sigma_first_try;
    float sigma_after_fail;
    bool  gate_update_applied;
} cos_v282_recovery_t;

typedef struct {
    cos_v282_action_t    action   [COS_V282_N_ACTION];
    cos_v282_chain_t     chain    [COS_V282_N_CHAIN];
    cos_v282_tool_row_t  tool     [COS_V282_N_TOOL];
    cos_v282_recovery_t  recovery [COS_V282_N_RECOVERY];

    float tau_auto;        /* 0.20 */
    float tau_ask;         /* 0.60 */
    float tau_chain;       /* 0.70 */
    float tau_tool_use;    /* 0.30 */
    float tau_tool_swap;   /* 0.60 */
    float math_eps;        /* 1e-4 */

    int   n_action_rows_ok;
    int   n_action_auto;
    int   n_action_ask;
    int   n_action_block;

    int   n_chain_rows_ok;
    int   n_chain_proceed;
    int   n_chain_abort;
    bool  chain_math_ok;

    int   n_tool_rows_ok;
    int   n_tool_use;
    int   n_tool_swap;
    int   n_tool_block;

    int   n_recovery_rows_ok;
    bool  recovery_monotone_ok;
    bool  recovery_updates_ok;

    float sigma_agent;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v282_state_t;

void   cos_v282_init(cos_v282_state_t *s, uint64_t seed);
void   cos_v282_run (cos_v282_state_t *s);

size_t cos_v282_to_json(const cos_v282_state_t *s,
                         char *buf, size_t cap);

int    cos_v282_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V282_AGENT_H */

/*
 * v249 σ-MCP — Model Context Protocol integration.
 *
 *   MCP is the Anthropic-originated standard that lets
 *   agents talk to tools over JSON-RPC 2.0.  v249 makes
 *   Creation OS MCP-compatible in both directions and
 *   adds the one thing the protocol itself does not
 *   ship: a typed σ on every tool call.
 *
 *   Two halves (v0 manifest, both strict):
 *
 *   Server side — Creation OS advertises 5 tools
 *   (exactly, canonical order):
 *     reason · plan · create · simulate · teach
 *   and 3 resources (exactly, canonical order):
 *     memory · ontology · skills
 *   over JSON-RPC 2.0.
 *
 *   Client side — Creation OS consumes 4 external
 *   servers (v0 fixture):
 *     database · api · filesystem · search
 *   Each external server carries a σ_trust ∈ [0, 1]
 *   measuring how reliable the provider has been in
 *   the lineage audit.
 *
 *   σ-gated tool use (v0 fixture: 5 tool calls, one
 *   per server-side tool, each returning a σ and a
 *   gate decision):
 *     σ ≤ τ_tool (0.40) → USE   (pass)
 *     σ >  τ_tool         → WARN (result attached,
 *                                 warning emitted,
 *                                 σ flagged upstream)
 *     σ >  τ_refuse(0.75) → REFUSE (no result used)
 *   Exactly one fixture call exceeds τ_tool to exercise
 *   the warn branch; exactly one exceeds τ_refuse to
 *   exercise the refuse branch — otherwise the merge-
 *   gate has no audit that the gating *has teeth*.
 *
 *   Tool discovery:
 *     Exactly 3 modes, in canonical order:
 *       local · mdns · registry
 *     Every mode reports `found_count ≥ 0` and
 *     `mode_ok == true`; ontology mapping via v169 is
 *     asserted as `ontology_mapping_ok`.
 *
 *   σ_mcp (surface hygiene):
 *       σ_mcp = 1 −
 *         (tools_ok + resources_ok + externals_ok +
 *          gate_decisions_ok + discovery_ok) /
 *         (5 + 3 + 4 + 5 + 3)
 *   v0 requires `σ_mcp == 0.0`.
 *
 *   Contracts (v0):
 *     1. jsonrpc_version == "2.0".
 *     2. Exactly 5 tools in canonical order, every
 *        `tool_ok == true`.
 *     3. Exactly 3 resources in canonical order, every
 *        `resource_ok == true`.
 *     4. Exactly 4 external servers in canonical order,
 *        every `σ_trust ∈ [0, 1]`.
 *     5. Exactly 5 tool-call fixtures, one per tool; at
 *        least 3 USE decisions AND ≥ 1 WARN AND ≥ 1
 *        REFUSE; every σ ∈ [0, 1]; gate decision
 *        matches thresholds byte-for-byte.
 *     6. Exactly 3 discovery modes in canonical order,
 *        every `mode_ok == true`; `ontology_mapping_ok`.
 *     7. σ_mcp ∈ [0, 1] AND σ_mcp == 0.0.
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v249.1 (named, not implemented): live JSON-RPC
 *     transport, real mDNS advertisement, live v169
 *     ontology-driven tool selection, persistent σ_trust
 *     lineage per external server, streamed tool
 *     results with incremental σ.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V249_MCP_H
#define COS_V249_MCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V249_N_TOOLS      5
#define COS_V249_N_RESOURCES  3
#define COS_V249_N_EXTERNALS  4
#define COS_V249_N_CALLS      5
#define COS_V249_N_DISCOVERY  3

typedef enum {
    COS_V249_GATE_USE    = 1,
    COS_V249_GATE_WARN   = 2,
    COS_V249_GATE_REFUSE = 3
} cos_v249_gate_t;

typedef struct {
    char  name[16];
    char  desc[48];
    bool  tool_ok;
} cos_v249_tool_t;

typedef struct {
    char  name[16];
    bool  resource_ok;
} cos_v249_resource_t;

typedef struct {
    char   name[16];
    char   endpoint[48];
    float  sigma_trust;
} cos_v249_external_t;

typedef struct {
    char             tool_name[16];
    float            sigma_result;
    cos_v249_gate_t  decision;
} cos_v249_call_t;

typedef struct {
    char  mode[12];    /* local | mdns | registry */
    int   found_count;
    bool  mode_ok;
} cos_v249_discovery_t;

typedef struct {
    char jsonrpc_version[6];

    cos_v249_tool_t       tools     [COS_V249_N_TOOLS];
    cos_v249_resource_t   resources [COS_V249_N_RESOURCES];
    cos_v249_external_t   externals [COS_V249_N_EXTERNALS];
    cos_v249_call_t       calls     [COS_V249_N_CALLS];
    cos_v249_discovery_t  discovery [COS_V249_N_DISCOVERY];

    float  tau_tool;
    float  tau_refuse;

    int    n_tools_ok;
    int    n_resources_ok;
    int    n_externals_ok;
    int    n_use, n_warn, n_refuse;
    int    n_discovery_ok;
    bool   ontology_mapping_ok;

    float  sigma_mcp;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v249_state_t;

void   cos_v249_init(cos_v249_state_t *s, uint64_t seed);
void   cos_v249_run (cos_v249_state_t *s);

size_t cos_v249_to_json(const cos_v249_state_t *s,
                         char *buf, size_t cap);

int    cos_v249_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V249_MCP_H */

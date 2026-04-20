/*
 * σ-MCP — Model Context Protocol (2026 standard), σ-gated.
 *
 * Creation OS speaks MCP both ways:
 *
 *   Server (cos mcp serve): any MCP-capable client (Claude Code,
 *   Cursor, Windsurf, …) can call three σ-primitives as tools:
 *     - sigma_measure     → float σ for arbitrary text
 *     - sigma_gate        → ACCEPT / RETHINK / ABSTAIN verdict
 *     - sigma_diagnostic  → structured reason for the σ value
 *
 *   Client (cos chat --mcp-server X): cos can wrap any external
 *   MCP server and apply σ to every tool-call response:
 *     - sigma_tool_selection  → did we pick the right tool?
 *     - sigma_arguments       → are the arguments plausible?
 *     - sigma_result          → is the returned payload trustworthy?
 *   Known attack: tool-provided prompt injection.  σ_result is the
 *   gate that stops the pipeline from blindly trusting MCP output.
 *
 * Wire format is JSON-RPC 2.0 over line-delimited stdio, matching
 * the official MCP stdio transport.  We ship our own minimal JSON
 * encoder / tokenizer so the kernel has zero external deps.
 *
 * Supported methods:
 *   - initialize
 *   - tools/list
 *   - tools/call
 *   - ping
 *
 * Errors:
 *   -32601 method_not_found
 *   -32602 invalid_params
 *   -32603 internal_error
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MCP_H
#define COS_SIGMA_MCP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_MCP_PROTOCOL_VERSION  "2026-03-26"
#define COS_MCP_LINE_MAX           4096
#define COS_MCP_PAYLOAD_MAX        8192
#define COS_MCP_MAX_TOOLS            16
#define COS_MCP_NAME_MAX             64

enum cos_mcp_error {
    COS_MCP_OK                =  0,
    COS_MCP_ERR_ARG           = -1,
    COS_MCP_ERR_PARSE         = -2,
    COS_MCP_ERR_METHOD        = -3,   /* maps to -32601 */
    COS_MCP_ERR_PARAMS        = -4,   /* maps to -32602 */
    COS_MCP_ERR_INTERNAL      = -5,   /* maps to -32603 */
    COS_MCP_ERR_SIGMA         = -6,   /* caller rejected by σ-gate */
};

/* ==================================================================
 * Server
 * ================================================================== */

typedef struct {
    char  name[COS_MCP_NAME_MAX];
    char  description[COS_MCP_NAME_MAX];
} cos_mcp_tool_t;

typedef struct cos_mcp_server {
    cos_mcp_tool_t tools[COS_MCP_MAX_TOOLS];
    int n_tools;
    int initialized;
    int next_request_id;
} cos_mcp_server_t;

void cos_mcp_server_init(cos_mcp_server_t *s);

/* Process a single JSON-RPC request line; write the JSON-RPC
 * response into `reply` (null-terminated, ≤ cap bytes).  Returns
 * COS_MCP_OK on success — a successful call may still produce a
 * JSON-RPC error envelope, but the C call itself is a success. */
int cos_mcp_server_handle(cos_mcp_server_t *s,
                          const char *request_line,
                          char *reply, size_t cap);

/* ==================================================================
 * Client
 * ================================================================== */

typedef struct {
    float sigma_tool_selection;  /* 0..1 — does the tool name fit? */
    float sigma_arguments;       /* 0..1 — do args look reasonable? */
    float sigma_result;          /* 0..1 — is the payload trustworthy? */
    int   admitted;              /* final ACCEPT / reject decision  */
    char  reason[128];
} cos_mcp_gate_t;

typedef struct {
    float tau_tool;              /* reject tool call if σ > τ        */
    float tau_args;
    float tau_result;
} cos_mcp_gate_thresholds_t;

void cos_mcp_gate_thresholds_default(cos_mcp_gate_thresholds_t *t);

/* Client-side σ-gate on a proposed MCP call:
 *   tool_name    — which server-declared tool we're about to invoke
 *   arguments    — JSON-serialised argument object (opaque string)
 *   result       — JSON-serialised response payload from the server
 * We produce three σ values + an admitted/blocked verdict.  The
 * kernel is heuristic by design (no network, no LLM) but the
 * signals are real: empty / oversized / injection-suspicious
 * payloads all bump σ. */
int cos_mcp_gate_evaluate(const char *tool_name,
                          const char *arguments,
                          const char *result,
                          const cos_mcp_gate_thresholds_t *tau,
                          cos_mcp_gate_t *out);

/* Build a JSON-RPC call envelope for `tool_name` with opaque
 * `arguments_json` (must already be a JSON object).  Returns
 * length written or -1. */
int cos_mcp_client_compose_tool_call(int request_id,
                                     const char *tool_name,
                                     const char *arguments_json,
                                     char *line, size_t cap);

/* ==================================================================
 * Tool implementations (exposed so the CLI can reuse them).
 * ================================================================== */

float cos_mcp_tool_sigma_measure(const char *text);
int   cos_mcp_tool_sigma_gate(float sigma, float tau, char *verdict, size_t cap);

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_mcp_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_MCP_H */

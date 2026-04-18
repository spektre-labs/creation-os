/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v116 σ-MCP — Model Context Protocol server.
 *
 * Implements the core MCP JSON-RPC 2.0 methods over stdio so Creation
 * OS appears as a local MCP server to hosts like Claude Desktop,
 * Cursor, VS Code, continue.dev, etc.  Methods covered:
 *
 *   initialize
 *   initialized                     (notification)
 *   tools/list
 *   tools/call
 *   resources/list
 *   resources/read
 *   prompts/list
 *   prompts/get
 *   ping
 *   shutdown
 *
 * Primitives exposed (v116.0):
 *
 *   Tools
 *     cos_chat            -> σ-gated chat completion (stub when no GGUF)
 *     cos_reason          -> v111.2 σ-Reason loop
 *     cos_memory_search   -> v115 episodic retrieval
 *     cos_sandbox_execute -> v113 σ-Sandbox execution
 *     cos_sigma_profile   -> current σ-state snapshot
 *
 *   Resources
 *     cos://memory/{session_id}
 *     cos://knowledge/{doc_id}
 *     cos://sigma/history
 *
 *   Prompts
 *     cos_analyze         -> "Analyze this with σ-governance"
 *     cos_verify          -> "Verify this claim, abstain if uncertain"
 *
 * The server is transport-agnostic at the public API level: it takes
 * a single JSON-RPC request string and returns the JSON response
 * string.  The CLI glues it to stdio (line-delimited JSON, same shape
 * used by mcp-python/mcp-typescript SDKs when running over stdio).
 *
 * σ enters every response that surfaces model output: the `result`
 * object carries a `sigma` block mirroring the v101 8-channel profile,
 * and if a σ-gate abstains, the response is an MCP-level `error` with
 * a structured `data.abstained_channel` so the host can decide whether
 * to ask for a better model or surface the doubt to the user.
 *
 * This header intentionally exposes only the orchestrator: concrete
 * backends (bridge*, memory store, sandbox, swarm) are injected via
 * the config struct so that stdio CLI, HTTP SSE wrapper, and unit
 * tests can all share the dispatcher.
 */
#ifndef COS_V116_MCP_SERVER_H
#define COS_V116_MCP_SERVER_H

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations — only v115 memory store is referenced by
 * header (type-safe); v101/v111/v113/v114 handles are passed as
 * `void *` to keep this header dependency-free.  The .c file uses
 * them only when the matching compile-time define is set. */
typedef struct cos_v115_store cos_v115_store_t;

typedef struct cos_v116_config {
    /* Server identity advertised in `initialize` response. */
    const char *server_name;            /* default "creation-os" */
    const char *server_version;         /* default "0.116.0" */

    /* Optional injected backends; if NULL, the matching tool replies
     * with a structured "unavailable" error so the host knows the
     * capability is present but not wired. */
    cos_v115_store_t *memory_store;     /* for cos_memory_search */
    void             *bridge;           /* v101 bridge (cos_chat,
                                           cos_reason, cos_sigma_profile) */
    void             *swarm_cfg;        /* v114 cos_v114_config_t *  */

    /* Transport hint, for diagnostics only.  "stdio" or "http". */
    const char *transport;
} cos_v116_config_t;

/* Fill a defaults config (names, version, transport="stdio"). */
void cos_v116_config_defaults(cos_v116_config_t *cfg);

/* Core dispatch: accepts one JSON-RPC 2.0 request, writes the JSON
 * response (or notification-suppressed empty string) into `out`.
 *
 * Returns bytes written (>=0), or -1 if the response would overflow
 * `out_cap`.  For notifications (no "id" field) returns 0 and writes
 * "".  This function is re-entrant and never touches file descriptors.
 */
int cos_v116_dispatch(const cos_v116_config_t *cfg,
                      const char *request, size_t request_len,
                      char *out, size_t out_cap);

/* Convenience loop: read one JSON object per line from `in`, write
 * responses (if any) one-per-line to `out`.  Returns on EOF or after
 * a graceful `shutdown` request.  Returns 0 on clean exit, negative
 * on fatal I/O errors. */
int cos_v116_run_stdio(const cos_v116_config_t *cfg, FILE *in, FILE *out);

/* Pure-C self-test.  No backends attached: verifies JSON-RPC shape,
 * initialize handshake, tools/list, resources/list, prompts/list,
 * and a simulated tools/call that returns an "unavailable" error.
 * Returns 0 on pass. */
int cos_v116_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V116_MCP_SERVER_H */

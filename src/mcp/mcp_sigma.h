/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v36: MCP-native σ server (JSON-RPC; lab; not merge-gate).
 */
#ifndef CREATION_OS_MCP_SIGMA_H
#define CREATION_OS_MCP_SIGMA_H

#include <stddef.h>

/** Handle one JSON-RPC line; write complete response into out (NUL-terminated). Returns response length or 0 if no reply (notification). */
size_t cos_mcp_handle_request(const char *json_line, char *out, size_t out_cap);

/** Minimal HTTP POST server (JSON-RPC body). Blocks; SIGINT to stop. */
void cos_mcp_http_serve(unsigned short port);

#endif /* CREATION_OS_MCP_SIGMA_H */

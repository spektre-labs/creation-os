# v116 — σ-MCP: Model Context Protocol Server

v116 exposes Creation OS as a **local MCP server** over JSON-RPC 2.0.
Claude Desktop, Cursor, VS Code, continue.dev, and every other MCP-
compliant host can now use Creation OS tools, resources, and prompts.
Every response that surfaces model output carries an 8-channel σ
profile — the host receives not just an answer but the doubt.

## The problem v116 solves

Hosts that implement MCP today (Anthropic’s reference servers, the
various community servers listed on modelcontextprotocol.io) return
*content* but not *confidence*. When the host's large model stitches
several MCP responses together, it cannot tell a confident answer
apart from a guess.

v116 annotates every tool response with σ telemetry under the
`result.sigma` key (or raises a structured MCP error with a
`data.abstained_channel` field when σ_product exceeds τ on the
Creation OS side). The host model sees the doubt and can decide
whether to trust it, pass it through, or ask for a better model.

## Capabilities advertised

### Tools (5)

| Name                  | What it does |
|-----------------------|--------------|
| `cos_chat`            | σ-gated chat completion (needs v101 bridge) |
| `cos_reason`          | v111.2 σ-Reason MCTS loop (needs bridge) |
| `cos_memory_search`   | v115 σ-weighted episodic retrieval |
| `cos_sandbox_execute` | v113 σ-Sandbox code execution |
| `cos_sigma_profile`   | Read-only 8-channel σ snapshot |

### Resources (3)

| URI                             | Content |
|---------------------------------|---------|
| `cos://memory/{session_id}`     | Session memory + chat history |
| `cos://knowledge/{doc_id}`      | Single knowledge-base entry |
| `cos://sigma/history`           | Recent σ-profiles |

### Prompts (2)

| Name          | Template |
|---------------|----------|
| `cos_analyze` | "Analyze the following with σ-governance..." |
| `cos_verify`  | "Verify the following claim. Return PASS/FAIL/ABSTAIN..." |

## API surface

JSON-RPC 2.0 over line-delimited stdio. Supported methods:

- `initialize` / `initialized`
- `ping` / `shutdown`
- `tools/list` · `tools/call`
- `resources/list` · `resources/read`
- `prompts/list` · `prompts/get`

`initialize` advertises:

```json
"capabilities": {
  "tools": { "listChanged": false },
  "resources": { "listChanged": false, "subscribe": false },
  "prompts": { "listChanged": false },
  "experimental": {
    "sigmaGovernance": {
      "enabled": true,
      "channels": ["entropy","sigma_mean","sigma_max_token",
                   "sigma_product","sigma_tail_mass",
                   "sigma_n_effective","margin","stability"]
    }
  }
}
```

Claude Desktop / Cursor / VS Code connect by adding to
`~/.claude/claude_desktop_config.json` (or their MCP equivalent):

```json
{
  "mcpServers": {
    "creation-os": {
      "command": "creation_os_v116_mcp",
      "args": ["--stdio"]
    }
  }
}
```

## Source

- [`src/v116/mcp_server.h`](../../src/v116/mcp_server.h)
- [`src/v116/mcp_server.c`](../../src/v116/mcp_server.c)
- [`src/v116/main.c`](../../src/v116/main.c) — CLI
- [`benchmarks/v116/check_v116_mcp_stdio_smoke.sh`](../../benchmarks/v116/check_v116_mcp_stdio_smoke.sh)

## Self-test

```bash
make check-v116
```

Runs `--self-test` (10 pure-C assertions covering JSON-RPC shape,
every advertised method, notification handling, and parse errors),
then a stdio smoke test that exercises `initialize`, `tools/list`,
`resources/list`, `prompts/list`, `prompts/get` with substitution,
`tools/call cos_sigma_profile`, and a structured-unavailable response
for `cos_memory_search` when no store is attached.

## Claim discipline

- v116.0 ships the **stdio transport**. HTTP SSE transport is
  scheduled for v116.1 (same dispatcher).
- Tool backends for `cos_chat`, `cos_reason`, and `cos_sandbox_execute`
  are advertised but produce structured `unavailable` errors when no
  backend is attached (stdio-only mode). The full dispatcher is wired
  to v101 / v111.2 / v113 when launched alongside the v106 HTTP layer
  with `COS_MCP_ATTACH_SERVER=1` (reserved for v116.1).
- We do not claim every MCP host has been validated end-to-end; the
  server passes the reference `modelcontextprotocol/mcp-client` schema
  tests at protocol version `2025-06-18` (tracked externally).

## σ-stack layer

v116 sits at the **Distribution** layer: it is how other hosts
*consume* Creation OS. Every response that crosses the MCP boundary
has σ attached by construction.

---

_Copyright (c) 2026 Spektre Labs · AGPL-3.0-or-later · Creation OS_

# Creation OS — MCP (σ-gate server)

English-only documentation for MCP host configuration. The σ-gate itself works without MCP; FastMCP is an optional transport.

## Install

```bash
pip install "creation-os[mcp]"
```

## CLI

- **stdio (default):** `cos mcp` — for Claude Desktop, Cursor, and other local MCP hosts.
- **HTTP (streamable):** `cos mcp --transport http --port 8000` — binds `127.0.0.1` for remote agents.

Registry helpers (unchanged): `cos mcp --list`, `cos mcp --wrap --server CMD`, `cos mcp --register --server-id ID`.

## Claude Desktop

```json
{
  "mcpServers": {
    "sigma-gate": {
      "command": "cos",
      "args": ["mcp"],
      "env": {}
    }
  }
}
```

Use the full path to `cos` if it is not on the MCP host `PATH`.

## Cursor (`.cursor/mcp.json`)

```json
{
  "mcpServers": {
    "sigma-gate": {
      "command": "cos",
      "args": ["mcp"]
    }
  }
}
```

## Tools (summary)

| Tool | Role |
|------|------|
| `score` | σ ∈ [0,1] + verdict for `(prompt, response)` |
| `score_cascade` | L1–L5 (and LSD when configured) per-level breakdown |
| `batch_score` | many pairs in one call; returns ``{"results": [...]}`` (stable MCP JSON shape) |
| `explain` | short natural-language note alongside σ |
| `graph_add` | σ-scored triple → lab `SigmaGraph` |

## Resources

- `config://thresholds` — JSON thresholds (`threshold_accept`, `threshold_abstain`).
- `evidence://ladder` — positive rows plus **explicit negatives** (e.g. HaluEval, not-AGI). See `docs/CLAIM_DISCIPLINE.md` before citing lab metrics in product copy.

## License

SPDX: `LicenseRef-SCSL-1.0 OR AGPL-3.0-only`.

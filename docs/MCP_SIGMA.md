# Creation OS as an MCP σ server (v36)

This document describes the **lab** binary `creation_os_mcp` (JSON-RPC over **STDIO** by default, optional **HTTP** shim).

## What it is

- **MCP-shaped** surface: `initialize`, `tools/list`, `tools/call`, `resources/list`, `resources/read`, `prompts/list`, `prompts/get`.
- **Tools**
  - `measure_sigma` — logits → decomposition + basic entropy/margin (and optional text presence flag).
  - `should_abstain` — Platt + coarse profile thresholds (strict/moderate/permissive) + top-margin trip.
  - `sigma_report` — eight-channel-shaped report (some fields are **0 / n/a** without a full engine hook).
- **Resources**
  - `sigma://session/current` — ring buffer of recent epistemic samples from `measure_sigma`.
  - `sigma://calibration/profile` — contents of `SIGMA_CALIBRATION` (escaped JSON text).
  - `sigma://thresholds` — contents of `SIGMA_THRESHOLDS` (escaped JSON text).
- **Prompts**
  - `sigma_aware_system` — short system copy that tells the client to call `measure_sigma` / respect `should_abstain`.

## Client wiring (examples in-repo)

| Client | Example config in this repo |
|--------|-----------------------------|
| Claude Desktop | `config/claude_desktop_config.json` |
| Cursor / other MCP hosts | Copy the `mcpServers` object from `config/claude_desktop_config.json` into your client config (e.g. workspace `.cursor/mcp.json` — **gitignored** here so secrets/paths stay local). |

Adjust `command` to an **absolute path** if your client does not start in the repository root.

## Positioning (I-tier)

| Old shape | New shape |
|-----------|-----------|
| Standalone demos only | **MCP server** other tools can attach to |
| One bespoke integration | **Many MCP clients** with one config line |
| “Kernel repo” framing | **“Install σ on any MCP-capable agent”** framing |

## Claim discipline

Full **Claude Desktop + TruthfulQA/GSM8K** A/B rows are **not M-tier** until an archived harness exists. See `benchmarks/v36/mcp_bench_stub.sh` and tier tags in `docs/WHAT_IS_REAL.md`.

## HTTP transport

`./creation_os_mcp --transport http --http-port 8765` accepts a single JSON-RPC object in the HTTP POST body and returns one JSON object (**no SSE yet**).

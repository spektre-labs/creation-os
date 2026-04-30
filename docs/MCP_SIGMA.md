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

## Python LSD σ-gate (stdio, MCP SDK)

For **hallucination-oriented scoring** on `(prompt, response)` pairs using the
lab **LSD pickle** (no `cos chat` subprocess), run the FastMCP server:

```bash
export PYTHONPATH=python
export SIGMA_PROBE_PATH=benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl
python3 -m cos.mcp_sigma_server
```

Install dependency: `pip install 'mcp[cli]'` (see `mcp.server.fastmcp.FastMCP`).

Tools: `verify_response`, `should_generate` (optional HF precheck via
`SIGMA_MCP_PRECHECK_MODEL`), `sigma_gate_audit_tail`, `sigma_gate_stats`.

**Response envelope (FastMCP):** each tool returns JSON shaped as
`{ "result": <payload>, "sigma": { "value", "verdict", "d_sigma", "k_eff", "signals" }, "metadata": { "model", "gate_version", "license" } }`.
`sigma.value` may be `null` when no scalar score applies (e.g. routing economics only).
See `python/cos/mcp_sigma_server.py` and `docs/MCP_LISTING.md`.

The legacy JSON-RPC server `scripts/cos_mcp_server.py` also exposes
`sigma_gate_verify`, `sigma_gate_audit_tail`, and `sigma_gate_stats` on the same
audit ring when the Python `cos` package is importable.

Gateway example: `configs/mcp/bifrost_sigma_gate.example.yaml`.

Audit entries are **in-process** retention hooks — not a legal compliance product.

## HTTP transport

`./creation_os_mcp --transport http --http-port 8765` accepts a single JSON-RPC object in the HTTP POST body and returns one JSON object (**no SSE yet**).

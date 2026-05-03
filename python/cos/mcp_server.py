# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Creation OS MCP Server — σ-gate as a tool for any MCP client (FastMCP 3.x).

Usage:
    stdio:  ``cos mcp`` (Claude Desktop, Cursor, local agents)
    http:   ``cos mcp --transport http`` (streamable HTTP; remote agents)

Install MCP transport: ``pip install 'creation-os[mcp]'`` (pulls FastMCP 3.x).
The σ-gate core (:class:`~cos.sigma_gate.SigmaGate`) needs no MCP.
"""
from __future__ import annotations

import json
import sys
from typing import Any

_FASTMCP_IMPORT_ERROR: str | None = None
try:  # pragma: no cover - exercised when fastmcp installed
    from fastmcp import FastMCP
except ImportError:
    try:
        from mcp.server.fastmcp import FastMCP
    except ImportError as exc:
        FastMCP = None  # type: ignore[misc, assignment]
        _FASTMCP_IMPORT_ERROR = str(exc)


def _verdict_str(verdict: object) -> str:
    raw = verdict.name if hasattr(verdict, "name") else str(verdict)
    if isinstance(raw, str) and "." in raw:
        return raw.rsplit(".", 1)[-1]
    return str(raw)


def _require_fastmcp() -> Any:
    if FastMCP is None:
        raise ImportError(
            "FastMCP / MCP SDK not installed. Install optional extra: "
            "pip install 'creation-os[mcp]'  "
            f"(import error: {_FASTMCP_IMPORT_ERROR or 'unknown'})"
        )
    return FastMCP


def build_mcp() -> Any:
    """Construct the FastMCP app (tools, resources, prompts)."""
    MCP = _require_fastmcp()
    mcp = MCP(
        "creation-os",
        instructions="σ-gate hallucination detection — tools score LLM outputs and graph edges.",
    )

    @mcp.tool()
    def score(prompt: str, response: str) -> dict[str, Any]:
        """Score a prompt-response pair. Returns σ ∈ [0,1] and verdict."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        sigma, verdict = gate.score(prompt, response)
        return {"sigma": round(float(sigma), 4), "verdict": _verdict_str(verdict)}

    @mcp.tool()
    def score_cascade(prompt: str, response: str) -> dict[str, Any]:
        """Score with full L1–L5 cascade where available. Returns per-level σ."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        return gate.score_cascade(prompt, response)

    @mcp.tool()
    def batch_score(pairs: list[dict[str, Any]]) -> dict[str, Any]:
        """Score multiple prompt-response pairs.

        Input: [{"prompt": "...", "response": "..."}, ...]

        Returns a dict with key ``results`` (MCP serializes bare lists inconsistently).
        """
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        results: list[dict[str, Any]] = []
        for pair in pairs:
            p = str(pair.get("prompt", ""))
            r = str(pair.get("response", ""))
            sigma, verdict = gate.score(p, r)
            results.append(
                {
                    "prompt": p,
                    "response": r,
                    "sigma": round(float(sigma), 4),
                    "verdict": _verdict_str(verdict),
                }
            )
        return {"results": results}

    @mcp.tool()
    def explain(prompt: str, response: str) -> dict[str, Any]:
        """Explain why σ-gate produced this verdict."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        sigma, verdict = gate.score(prompt, response)
        vn = _verdict_str(verdict)
        if vn == "ACCEPT":
            note = "Response appears reliable."
        elif vn == "RETHINK":
            note = "Response needs verification."
        elif vn == "ABSTAIN":
            note = "Response is unreliable — abstaining."
        else:
            note = f"Verdict {vn}."
        return {
            "sigma": round(float(sigma), 4),
            "verdict": vn,
            "explanation": (f"σ={float(sigma):.3f}. " + note),
        }

    @mcp.tool()
    def graph_add(subject: str, relation: str, obj: str) -> dict[str, Any]:
        """Add a σ-scored triplet to the knowledge graph (honors graph write threshold)."""
        from cos.graph import SigmaGraph
        from cos.sigma_gate import SigmaGate

        statement = f"{subject} {relation} {obj}"
        gate = SigmaGate()
        kg = SigmaGraph(gate=gate)
        sigma, verdict = gate.score(f"Is it true that {statement}?", statement)
        out = kg.add(subject, relation, obj, sigma=float(sigma))
        payload: dict[str, Any] = {
            "added": bool(out.get("added", False)),
            "sigma": round(float(sigma), 4),
            "verdict": _verdict_str(verdict),
        }
        if not payload["added"] and out.get("reason"):
            payload["reason"] = out["reason"]
        return payload

    @mcp.resource("config://thresholds")
    def get_thresholds() -> str:
        """Current σ-gate threshold configuration."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        return json.dumps(
            {
                "accept": gate.threshold_accept,
                "abstain": gate.threshold_abstain,
                "threshold_accept": gate.threshold_accept,
                "threshold_abstain": gate.threshold_abstain,
                "version": "1.0.0",
            }
        )

    @mcp.resource("evidence://ladder")
    def get_evidence_ladder() -> str:
        """Evidence ladder — strengths, saturation warnings, and explicit failures."""
        return """
POSITIVE:
- TruthfulQA AUROC 0.982 (saturated / ceiling-limited; interpret with care)
- TriviaQA AUROC 0.960
NEGATIVE:
- HaluEval AUROC 0.514 (fail; not a reliability claim)
- NOT AGI ACHIEVED
"""

    @mcp.prompt()
    def verify_output(text: str) -> str:
        """Template: ask σ-gate to verify any LLM output."""
        return f"Please verify this output for hallucinations:\n\n{text}"

    return mcp


def run_server(transport: str = "stdio", *, port: int = 8000) -> None:
    """Run the MCP server (blocking)."""
    app = build_mcp()
    t = (transport or "stdio").strip().lower().replace("_", "-")
    if t in ("http", "streamable-http", "streamablehttp"):
        # MCP streamable HTTP (remote agents)
        app.run(transport="streamable-http", host="127.0.0.1", port=int(port))
    else:
        app.run(transport="stdio")


def main() -> None:
    run_server()


if __name__ == "__main__":
    if FastMCP is None:
        print(
            "cos mcp_server: install FastMCP: pip install 'creation-os[mcp]'",
            file=sys.stderr,
        )
        sys.exit(1)
    main()

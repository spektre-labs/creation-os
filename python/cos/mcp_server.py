# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Creation OS MCP Server — σ-gate as tools/resources for any MCP client.

Prefer the official MCP SDK (:mod:`mcp.server.fastmcp`); fall back to the
standalone ``fastmcp`` package (Prefect) when the SDK is absent.

Usage:
    stdio: ``cos mcp``
    streamable HTTP: ``cos mcp --transport http --port 8000``

Install: ``pip install 'creation-os[mcp]'`` (optional; core σ-gate needs no MCP).
"""
from __future__ import annotations

import json
import sys
from typing import Any

_HAS_MCP = False
_MCP_BACKEND = "none"  # "official" | "standalone"
_MCP_IMPORT_ERROR: str | None = None
FastMCP = None  # type: ignore[misc, assignment]

try:  # pragma: no cover - import path depends on extras
    from mcp.server.fastmcp import FastMCP as _FastMCP

    FastMCP = _FastMCP
    _HAS_MCP = True
    _MCP_BACKEND = "official"
except ImportError as exc_official:
    try:
        from fastmcp import FastMCP as _FastMCP

        FastMCP = _FastMCP
        _HAS_MCP = True
        _MCP_BACKEND = "standalone"
    except ImportError as exc_standalone:
        _MCP_IMPORT_ERROR = str(exc_standalone or exc_official)

_MCP_INSTRUCTIONS = (
    "σ-gate hallucination detection for any LLM output. "
    "Measures the gap between what a model claims and what is reliable."
)

EVIDENCE_LADDER_TEXT = (
    "POSITIVE:\n"
    "  TruthfulQA AUROC 0.982 (saturated since 2024; interpret with care)\n"
    "  TriviaQA AUROC 0.960\n"
    "NEGATIVE:\n"
    "  HaluEval AUROC 0.514 (fail; not a reliability claim)\n"
    "  NOT AGI ACHIEVED"
)


def _verdict_str(verdict: object) -> str:
    raw = verdict.name if hasattr(verdict, "name") else str(verdict)
    if isinstance(raw, str) and "." in raw:
        return raw.rsplit(".", 1)[-1]
    return str(raw)


def _require_mcp() -> Any:
    if not _HAS_MCP or FastMCP is None:
        raise ImportError(
            "MCP SDK not installed. Run: pip install 'creation-os[mcp]'  "
            f"(import error: {_MCP_IMPORT_ERROR or 'unknown'})"
        )
    return FastMCP


def _register_components(mcp: Any) -> Any:
    """Attach tools/resources/prompts to a FastMCP instance (official or standalone)."""

    @mcp.tool()
    def score(prompt: str, response: str) -> dict[str, Any]:
        """Score a prompt-response pair for hallucination.

        Returns σ ∈ [0, 1] and verdict (ACCEPT / RETHINK / ABSTAIN). Low σ = more reliable.
        """
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        sigma, verdict = gate.score(prompt, response)
        return {"sigma": round(float(sigma), 4), "verdict": _verdict_str(verdict)}

    @mcp.tool()
    def score_cascade(prompt: str, response: str) -> dict[str, Any]:
        """Score with full L1–L5 cascade where available (L1 entropy; L2–L5 need hidden states)."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        return gate.score_cascade(prompt, response)

    @mcp.tool()
    def batch_score(pairs: list[dict[str, Any]]) -> dict[str, Any]:
        """Score multiple prompt-response pairs.

        Input: [{"prompt": "...", "response": "..."}, ...]

        Returns ``{"results": [...]}`` — each entry is
        ``{prompt, response, sigma, verdict}``. (Plain lists are wrapped as
        ``{"result": ...}`` by some MCP runtimes; a dict keeps the key stable.)
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
        """Explain why σ-gate produced this verdict (σ, verdict, short rationale)."""
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        sigma, verdict = gate.score(prompt, response)
        vn = _verdict_str(verdict)
        explanations = {
            "ACCEPT": "Response appears reliable — σ below accept threshold.",
            "RETHINK": "Response needs verification — σ in uncertain zone.",
            "ABSTAIN": "Response is unreliable — σ above abstain threshold.",
        }
        return {
            "sigma": round(float(sigma), 4),
            "verdict": vn,
            "explanation": explanations.get(vn, "Unknown verdict."),
        }

    @mcp.resource("config://thresholds")
    def get_thresholds() -> str:
        """Current σ-gate threshold configuration."""
        from cos.config import DEFAULT_CONFIG
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        return json.dumps(
            {
                "threshold_accept": gate.threshold_accept,
                "threshold_abstain": gate.threshold_abstain,
                "default_threshold_accept": DEFAULT_CONFIG.threshold_accept,
                "default_threshold_abstain": DEFAULT_CONFIG.threshold_abstain,
                "version": "1.0.0",
            }
        )

    @mcp.resource("evidence://ladder")
    def get_evidence_ladder() -> str:
        """Evidence ladder — positives, saturation note, and explicit negative results (always)."""
        return EVIDENCE_LADDER_TEXT

    @mcp.prompt()
    def verify(text: str) -> str:
        """Ask σ-gate to verify any LLM output for hallucinations."""
        return f"Please score this output for hallucinations:\n\n{text}"

    return mcp


def create_mcp_server(*, host: str = "127.0.0.1", port: int = 8000) -> Any:
    """Build the FastMCP app (tools, resources, prompts). Only when an MCP backend is importable."""
    MCP = _require_mcp()
    if _MCP_BACKEND == "official":
        mcp = MCP(
            "creation-os-sigma-gate",
            instructions=_MCP_INSTRUCTIONS,
            host=host,
            port=port,
        )
    else:
        mcp = MCP(
            "creation-os-sigma-gate",
            instructions=_MCP_INSTRUCTIONS,
        )
    return _register_components(mcp)


def build_mcp(*, host: str = "127.0.0.1", port: int = 8000) -> Any:
    """Backward-compatible alias for :func:`create_mcp_server`."""
    return create_mcp_server(host=host, port=port)


def run_server(transport: str = "stdio", *, host: str = "127.0.0.1", port: int = 8000) -> None:
    """Run the MCP server (blocking)."""
    t = (transport or "stdio").strip().lower().replace("_", "-")
    is_http = t in ("http", "streamable-http", "streamablehttp")

    if is_http:
        app = create_mcp_server(host=host, port=port)
        if _MCP_BACKEND == "official":
            app.run(transport="streamable-http")
        else:
            app.run(transport="streamable-http", host=host, port=port)
        return

    app = create_mcp_server(host=host, port=port)
    if _MCP_BACKEND == "official":
        app.run()
    else:
        app.run(transport="stdio")


def main() -> None:
    run_server()


if __name__ == "__main__":
    if not _HAS_MCP:
        print(
            "cos mcp_server: install MCP extras: pip install 'creation-os[mcp]'",
            file=sys.stderr,
        )
        sys.exit(1)
    main()

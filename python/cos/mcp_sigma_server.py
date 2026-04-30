# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-gate MCP server (stdio): LSD ``SigmaGate`` scoring as MCP tools.

Uses the standard Python **MCP** SDK (``from mcp.server.fastmcp import FastMCP``).
Install in the lab venv::

    pip install 'mcp[cli]'

Environment:

* ``SIGMA_PROBE_PATH`` — path to ``sigma_gate_lsd.pkl`` (defaults to holdout, then full).
* ``SIGMA_MCP_PRECHECK_MODEL`` — optional HF id (e.g. ``gpt2``). When set, ``should_generate``
  loads the model once and runs ``SigmaPrecheck`` entropy mode (network + RAM).
* ``SIGMA_COST_ROUTING_STATS_JSON`` — optional path to ``router_summary.json`` for ``sigma_cost_report``.
* ``COS_TOKEN_PRICING_JSON`` — optional JSON object (model id → USD per 1K tokens) merged into defaults.

**verify_response** does **not** need a live causal LM: the LSD bundle runs the probe
checkpoint internally (``model`` may be ``None``).

Every tool returns the same **envelope**: ``result`` (tool payload), ``sigma``
(machine-readable risk: ``value``, ``verdict``, ``d_sigma``, ``k_eff``, ``signals``),
and ``metadata`` (``model``, ``gate_version``, SPDX ``license``). Optional
``SIGMA_MCP_GATE_VERSION`` overrides the default gate version string.

Audit entries are stored in ``mcp_sigma_audit`` (in-process ring buffer). See
``docs/CLAIM_DISCIPLINE.md`` before citing lab AUROCs in product copy.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Any, Dict, Optional, Tuple, Union

try:
    from mcp.server.fastmcp import FastMCP
except ImportError:  # pragma: no cover
    FastMCP = None  # type: ignore[misc, assignment]

from .mcp_sigma_audit import append_record, stats as audit_stats, tail as audit_tail
from .sigma_gate import SigmaGate
from .sigma_gate_core import Q16, SigmaState, Verdict, sigma_gate, sigma_update
from .sigma_gate_precheck import SigmaPrecheck
from .sigma_router import calculate_savings

# MCP response envelope (machine-readable σ metadata). Version override: SIGMA_MCP_GATE_VERSION.
_GATE_VERSION = (os.environ.get("SIGMA_MCP_GATE_VERSION") or "0.57").strip()
_LICENSE_SPDX = "LicenseRef-SCSL-1.0 OR AGPL-3.0-only"


def _q16_slope(q: int) -> float:
    """Interpret Q16.16 delta as a slope in σ-space (harness convention, not a second probe)."""
    return float(max(-4.0, min(4.0, float(q) / float(Q16))))


def _k_eff_unit(k_eff_q16: int) -> float:
    return float(max(0.0, min(1.0, float(k_eff_q16) / float(Q16))))


def _sigma_payload(
    *,
    value: Optional[float],
    verdict: Union[str, Verdict],
    d_sigma_q16: int,
    k_eff_q16: int,
    signals: Dict[str, Any],
) -> Dict[str, Any]:
    vn = verdict.name if isinstance(verdict, Verdict) else str(verdict)
    out: Dict[str, Any] = {
        "verdict": vn,
        "d_sigma": _q16_slope(d_sigma_q16),
        "k_eff": _k_eff_unit(k_eff_q16),
        "signals": dict(signals),
    }
    if value is None:
        out["value"] = None
    else:
        out["value"] = float(max(0.0, min(1.0, float(value))))
    return out


def _cognitive_state(sigma_01: float) -> Tuple[SigmaState, Verdict]:
    st = SigmaState()
    s = float(max(0.0, min(1.0, float(sigma_01))))
    sigma_update(st, s, max(0.0, min(1.0, 1.0 - s)))
    return st, sigma_gate(st)


def _sigma_neutral_signals() -> Dict[str, Any]:
    return {"lsd": None, "hide": None, "entropy": None}


def _mcp_envelope(result: Any, sigma_block: Dict[str, Any], model: str) -> Dict[str, Any]:
    return {
        "result": result,
        "sigma": sigma_block,
        "metadata": {
            "model": str(model),
            "gate_version": _GATE_VERSION,
            "license": _LICENSE_SPDX,
        },
    }


def _sigma_neutral() -> Dict[str, Any]:
    return _sigma_payload(
        value=None,
        verdict="ACCEPT",
        d_sigma_q16=0,
        k_eff_q16=Q16,
        signals=_sigma_neutral_signals(),
    )


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_routing_stats_for_cost() -> Dict[str, Any]:
    """Optional ``router_summary.json`` from eval (path in ``SIGMA_COST_ROUTING_STATS_JSON``)."""
    raw = (os.environ.get("SIGMA_COST_ROUTING_STATS_JSON") or "").strip()
    if not raw:
        return {}
    p = Path(raw).expanduser()
    if not p.is_absolute():
        p = (_repo_root() / p).resolve()
    if not p.is_file():
        return {}
    import json

    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def _default_probe_path() -> Path:
    env = (os.environ.get("SIGMA_PROBE_PATH") or "").strip()
    if env:
        p = Path(env).expanduser()
        if not p.is_absolute():
            p = (_repo_root() / p).resolve()
        return p
    for rel in (
        "benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl",
        "benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl",
    ):
        cand = _repo_root() / rel
        if cand.is_file():
            return cand.resolve()
    raise FileNotFoundError(
        "No LSD probe pickle; set SIGMA_PROBE_PATH or place sigma_gate_lsd.pkl under results_holdout or results_full"
    )


_gate: Optional[SigmaGate] = None
_precheck_model_id: str = ""
_model: Any = None
_tokenizer: Any = None
_precheck: Optional[SigmaPrecheck] = None


def _get_gate() -> SigmaGate:
    global _gate
    if _gate is None:
        _gate = SigmaGate(_default_probe_path())
    return _gate


def _ensure_precheck_stack(model_id: str) -> Tuple[SigmaPrecheck, Any, Any]:
    """Lazy HF model + tokenizer + SigmaPrecheck (optional path)."""
    global _precheck_model_id, _model, _tokenizer, _precheck
    if not model_id.strip():
        raise RuntimeError("precheck_model_empty")
    if _precheck is not None and _precheck_model_id == model_id:
        assert _model is not None and _tokenizer is not None
        return _precheck, _model, _tokenizer

    from transformers import AutoModelForCausalLM, AutoTokenizer

    mid = model_id.strip()
    tok = AutoTokenizer.from_pretrained(mid)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    m = AutoModelForCausalLM.from_pretrained(mid)
    m.eval()
    pc = SigmaPrecheck(tau_skip=0.85, mode="entropy")
    _precheck_model_id = mid
    _tokenizer = tok
    _model = m
    _precheck = pc
    return pc, m, tok


def _build_mcp() -> "FastMCP":
    if FastMCP is None:
        raise ImportError(
            "Install the MCP SDK: pip install 'mcp[cli]'  (provides mcp.server.fastmcp.FastMCP)"
        )

    mcp = FastMCP("creation-os-sigma-gate")

    @mcp.tool()
    def verify_response(
        prompt: str,
        response: str,
        reference: str = "",
        model: str = "unknown",
    ) -> Dict[str, Any]:
        """
        Score (prompt, response) with the LSD σ-gate pickle. Returns a **structured
        envelope**: ``result`` (tool payload), ``sigma`` (machine-readable risk block),
        ``metadata`` (model tag, gate version, SPDX license). Logged to the audit ring.
        """
        gate = _get_gate()
        ref = (reference or "").strip() or None
        p = (prompt or "").strip()
        r = (response or "").strip()
        sigma = float(gate.compute_sigma(None, None, p, r, reference=ref))
        st, verdict = _cognitive_state(sigma)
        sig = _sigma_payload(
            value=sigma,
            verdict=verdict,
            d_sigma_q16=int(st.d_sigma),
            k_eff_q16=int(st.k_eff),
            signals={"lsd": float(sigma), "hide": None, "entropy": None},
        )
        inner = {
            "reference_used": bool(ref),
            "message": "LSD trajectory probe score (hallucination-oriented risk in [0,1]).",
        }
        append_record(
            {
                "tool": "verify_response",
                "sigma": float(sigma),
                "decision": verdict.name,
                "model_tag": str(model),
            }
        )
        return _mcp_envelope(inner, sig, str(model))

    @mcp.tool()
    def should_generate(prompt: str, model: str = "unknown") -> Dict[str, Any]:
        """
        Optional pre-generation entropy signal on the prompt only. Requires
        ``SIGMA_MCP_PRECHECK_MODEL`` (e.g. gpt2); otherwise returns a neutral stub
        with an explanatory note (no weights downloaded).
        """
        mid = (os.environ.get("SIGMA_MCP_PRECHECK_MODEL") or "").strip()
        if not mid:
            inner = {
                "should_generate": True,
                "sigma_pre": None,
                "note": "Set SIGMA_MCP_PRECHECK_MODEL=gpt2 (or another HF causal LM) to enable entropy precheck.",
            }
            return _mcp_envelope(inner, _sigma_neutral(), str(model))
        try:
            pc, m, tok = _ensure_precheck_stack(mid)
            ok, sigma_pre = pc.should_generate(m, tok, (prompt or "").strip())
        except Exception as e:
            inner = {
                "should_generate": True,
                "sigma_pre": None,
                "error": f"precheck_failed:{type(e).__name__}:{e}",
            }
            return _mcp_envelope(inner, _sigma_neutral(), str(model))
        st, v = _cognitive_state(float(sigma_pre))
        sig = _sigma_payload(
            value=float(sigma_pre),
            verdict=v,
            d_sigma_q16=int(st.d_sigma),
            k_eff_q16=int(st.k_eff),
            signals={"lsd": None, "hide": None, "entropy": float(sigma_pre)},
        )
        inner = {
            "should_generate": bool(ok),
            "sigma_pre": float(sigma_pre),
            "precheck_hf_model": mid,
        }
        append_record({"tool": "should_generate", "sigma_pre": float(sigma_pre), "should_generate": ok})
        return _mcp_envelope(inner, sig, str(model))

    @mcp.tool()
    def sigma_gate_audit_tail(last_n: int = 20) -> Dict[str, Any]:
        """Return the last ``last_n`` audit records from this process (envelope)."""
        entries = audit_tail(int(last_n))
        return _mcp_envelope({"entries": entries}, _sigma_neutral(), "audit")

    @mcp.tool()
    def sigma_gate_stats() -> Dict[str, Any]:
        """Aggregate stats over the in-memory audit ring (envelope)."""
        return _mcp_envelope(audit_stats(), _sigma_neutral(), "audit")

    @mcp.tool()
    def sigma_cost_report(
        period_days: int = 30,
        daily_query_volume: int = 1000,
        routing_stats_json: str = "",
        avg_tokens_per_query: int = 300,
        baseline_model: str = "gpt-4o",
    ) -> Dict[str, Any]:
        """
        Illustrative routing economics vs sending every query to ``baseline_model``.

        Supply observed route counts (``router_summary.json`` from ``run_router_eval.py``)
        as JSON in ``routing_stats_json``, or set env ``SIGMA_COST_ROUTING_STATS_JSON`` to
        that file path. Without stats, returns a note and no savings claim.
        """
        import json

        disclaimer = (
            "Illustrative USD math from configurable token rates (see COS_TOKEN_PRICING_JSON); "
            "not live billing or a guaranteed savings figure (docs/CLAIM_DISCIPLINE.md)."
        )
        stats: Dict[str, Any] = {}
        if (routing_stats_json or "").strip():
            try:
                stats = json.loads(routing_stats_json)
            except json.JSONDecodeError as e:
                inner = {
                    "disclaimer": disclaimer,
                    "error": f"routing_stats_json_invalid:{e}",
                }
                return _mcp_envelope(inner, _sigma_neutral(), str(baseline_model))
        else:
            stats = _load_routing_stats_for_cost()

        total_scaled = max(0, int(daily_query_volume)) * max(0, int(period_days))
        if not stats or int(stats.get("total", 0) or 0) < 1:
            inner = {
                "disclaimer": disclaimer,
                "note": "No routing stats (pass routing_stats_json or set SIGMA_COST_ROUTING_STATS_JSON).",
                "period_days": int(period_days),
                "daily_query_volume": int(daily_query_volume),
                "scaled_queries": total_scaled,
            }
            return _mcp_envelope(inner, _sigma_neutral(), str(baseline_model))

        scaled = max(1, total_scaled)
        base = calculate_savings(
            scaled,
            stats,
            avg_tokens_per_query=int(avg_tokens_per_query),
            baseline_model=str(baseline_model),
        )
        out: Dict[str, Any] = {
            "disclaimer": disclaimer,
            "period_days": int(period_days),
            "daily_query_volume": int(daily_query_volume),
            "scaled_queries": scaled,
            "routing_observed_total": int(stats.get("total", 0) or 0),
            "savings": base,
        }
        for label, daily in (
            ("startup_1k", 1_000),
            ("smb_10k", 10_000),
            ("enterprise_100k", 100_000),
            ("large_enterprise_1m", 1_000_000),
        ):
            out[f"projection_{label}_annual_savings_usd"] = calculate_savings(
                int(daily) * 365,
                stats,
                avg_tokens_per_query=int(avg_tokens_per_query),
                baseline_model=str(baseline_model),
            )["savings_usd"]
        return _mcp_envelope(out, _sigma_neutral(), str(baseline_model))

    return mcp


def main() -> None:
    mcp = _build_mcp()
    mcp.run(transport="stdio")


if __name__ == "__main__":
    if FastMCP is None:
        print(
            "mcp_sigma_server: install MCP SDK: pip install 'mcp[cli]'",
            file=sys.stderr,
        )
        sys.exit(1)
    main()

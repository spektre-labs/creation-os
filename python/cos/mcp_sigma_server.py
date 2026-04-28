"""
σ-gate MCP server (stdio): LSD ``SigmaGate`` scoring as MCP tools.

Uses the standard Python **MCP** SDK (``from mcp.server.fastmcp import FastMCP``).
Install in the lab venv::

    pip install 'mcp[cli]'

Environment:

* ``SIGMA_PROBE_PATH`` — path to ``sigma_gate_lsd.pkl`` (defaults to holdout, then full).
* ``SIGMA_MCP_PRECHECK_MODEL`` — optional HF id (e.g. ``gpt2``). When set, ``should_generate``
  loads the model once and runs ``SigmaPrecheck`` entropy mode (network + RAM).

**verify_response** does **not** need a live causal LM: the LSD bundle runs the probe
checkpoint internally (``model`` may be ``None``).

Audit entries are stored in ``mcp_sigma_audit`` (in-process ring buffer). See
``docs/CLAIM_DISCIPLINE.md`` before citing lab AUROCs in product copy.
"""
from __future__ import annotations

import os
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

try:
    from mcp.server.fastmcp import FastMCP
except ImportError:  # pragma: no cover
    FastMCP = None  # type: ignore[misc, assignment]

from .mcp_sigma_audit import append_record, stats as audit_stats, tail as audit_tail
from .sigma_gate import SigmaGate
from .sigma_gate_precheck import SigmaPrecheck


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


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
        Score (prompt, response) with the LSD σ-gate pickle. Returns sigma in [0,1]
        and ACCEPT / RETHINK / ABSTAIN. Logged to the in-memory audit ring.
        """
        gate = _get_gate()
        ref = (reference or "").strip() or None
        sigma, decision = gate(None, None, (prompt or "").strip(), (response or "").strip(), reference=ref)
        out = {
            "sigma": float(sigma),
            "decision": str(decision),
            "model_tag": str(model),
            "reference_used": bool(ref),
        }
        append_record(
            {
                "tool": "verify_response",
                "sigma": float(sigma),
                "decision": str(decision),
                "model_tag": str(model),
            }
        )
        return out

    @mcp.tool()
    def should_generate(prompt: str, model: str = "unknown") -> Dict[str, Any]:
        """
        Optional pre-generation entropy signal on the prompt only. Requires
        ``SIGMA_MCP_PRECHECK_MODEL`` (e.g. gpt2); otherwise returns a neutral stub
        with an explanatory note (no weights downloaded).
        """
        mid = (os.environ.get("SIGMA_MCP_PRECHECK_MODEL") or "").strip()
        if not mid:
            return {
                "should_generate": True,
                "sigma_pre": None,
                "model_tag": str(model),
                "note": "Set SIGMA_MCP_PRECHECK_MODEL=gpt2 (or another HF causal LM) to enable entropy precheck.",
            }
        try:
            pc, m, tok = _ensure_precheck_stack(mid)
            ok, sigma_pre = pc.should_generate(m, tok, (prompt or "").strip())
        except Exception as e:
            return {
                "should_generate": True,
                "sigma_pre": None,
                "model_tag": str(model),
                "error": f"precheck_failed:{type(e).__name__}:{e}",
            }
        out = {
            "should_generate": bool(ok),
            "sigma_pre": float(sigma_pre),
            "model_tag": str(model),
            "precheck_hf_model": mid,
        }
        append_record({"tool": "should_generate", "sigma_pre": float(sigma_pre), "should_generate": ok})
        return out

    @mcp.tool()
    def sigma_gate_audit_tail(last_n: int = 20) -> List[Dict[str, Any]]:
        """Return the last ``last_n`` audit records from this process."""
        return audit_tail(int(last_n))

    @mcp.tool()
    def sigma_gate_stats() -> Dict[str, Any]:
        """Aggregate stats over the in-memory audit ring."""
        return audit_stats()

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

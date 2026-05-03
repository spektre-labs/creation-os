# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-trust metadata attached to MCP tool payloads (lab envelope).

Clients can inspect ``sigma_trust`` before acting on ``content``. This is **not** a wire
security boundary by itself — pair with TLS, auth, and :mod:`cos.sigma_mcp_firewall`.
"""
from __future__ import annotations

import os
from datetime import datetime, timezone
from typing import Any, Dict, Mapping, Optional, Union

# Aligns with ``SIGMA_MCP_GATE_VERSION`` / :mod:`cos.mcp_sigma_server` convention.
_GATE_VER = (os.environ.get("SIGMA_MCP_GATE_VERSION") or "0.57").strip()


def _now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def get_cascade_level(sigma: float) -> int:
    """Map σ in [0,1] to an informational cascade depth (1..5) for dashboards."""
    s = float(sigma)
    if s < 0.15:
        return 1
    if s < 0.30:
        return 2
    if s < 0.50:
        return 3
    if s < 0.70:
        return 4
    return 5


class SigmaMCPTrust:
    """Build and validate σ-trust objects carried alongside MCP results."""

    def __init__(self, *, gate_version: Optional[str] = None) -> None:
        self.gate_version = (gate_version or _GATE_VER).strip()

    def wrap_response(
        self,
        response: Union[str, Dict[str, Any]],
        sigma: float,
        verdict: str,
        *,
        extra: Optional[Mapping[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Return a dict suitable for merging into a tool JSON result."""
        trust = {
            "sigma": float(max(0.0, min(1.0, float(sigma)))),
            "verdict": str(verdict),
            "gate_version": self.gate_version,
            "cascade_level": get_cascade_level(float(sigma)),
            "timestamp": _now_iso(),
        }
        if extra:
            trust = {**trust, **dict(extra)}
        if isinstance(response, dict):
            out = dict(response)
            out["sigma_trust"] = trust
            return out
        return {"content": str(response), "sigma_trust": trust}

    def validate_incoming(self, mcp_message: Mapping[str, Any]) -> Dict[str, Any]:
        """Return whether upstream σ-trust is acceptable for local use."""
        trust = mcp_message.get("sigma_trust")
        if not isinstance(trust, dict):
            return {"trusted": False, "reason": "missing sigma_trust metadata"}

        ver = str(trust.get("verdict", "")).upper()
        if ver == "ABSTAIN":
            return {"trusted": False, "reason": "source ABSTAINED"}

        try:
            s = float(trust.get("sigma", 1.0))
        except (TypeError, ValueError):
            s = 1.0
        if s > 0.5:
            return {"trusted": False, "reason": f"high sigma: {s:.4f}"}

        return {"trusted": True, "sigma": s, "verdict": ver}


def merge_trust(
    base: Dict[str, Any],
    sigma: float,
    verdict: str,
    *,
    trust: Optional[SigmaMCPTrust] = None,
) -> Dict[str, Any]:
    """Attach ``sigma_trust`` to an existing MCP result dict."""
    t = trust or SigmaMCPTrust()
    return t.wrap_response(base, sigma, verdict)


__all__ = ["SigmaMCPTrust", "get_cascade_level", "merge_trust"]

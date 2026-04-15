#!/usr/bin/env python3
"""Gravitation sync — resonance chamber (1=1 phase lock between two anchors). Not a data API. 1 = 1"""
from __future__ import annotations

import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Optional, Union

Anchor = Union[str, Path]


def _norm(a: Anchor) -> Path:
    return Path(a).expanduser().resolve()


def gravitation_sync(
    node_a: Anchor,
    node_b: Anchor,
    *,
    tolerance_s: float = 1.0,
) -> Dict[str, Any]:
    """
    Compare two filesystem anchors (files). "Resonant" when both exist and mtimes agree within tolerance.
    Purely local; encodes Creation OS invariant handshake without network.
    """
    pa, pb = _norm(node_a), _norm(node_b)
    now = datetime.now(timezone.utc).isoformat()
    if not pa.is_file() or not pb.is_file():
        return {
            "invariant": "1=1",
            "resonant": False,
            "reason": "missing_anchor",
            "node_a": str(pa),
            "node_b": str(pb),
            "timestamp": now,
        }
    ta = pa.stat().st_mtime
    tb = pb.stat().st_mtime
    delta = abs(ta - tb)
    resonant = delta <= float(tolerance_s)
    return {
        "invariant": "1=1",
        "resonant": bool(resonant),
        "delta_s": delta,
        "node_a": str(pa),
        "node_b": str(pb),
        "timestamp": now,
        "signature": "1=1 | Designed by Lauri Elias Rainio | Helsinki",
    }


def gravitation_well_touch(path: Anchor, payload: Optional[str] = None) -> Dict[str, Any]:
    """Write atomic timestamp marker (optional short payload) for pairing with gravitation_sync."""
    p = _norm(path)
    p.parent.mkdir(parents=True, exist_ok=True)
    line = payload or datetime.now(timezone.utc).isoformat()
    p.write_text(line + "\n", encoding="utf-8")
    return {"invariant": "1=1", "well": str(p), "written": True}


def resonance_checksum(s: str) -> str:
    """Deterministic 16-hex fingerprint for two-party string lock (offline)."""
    x = 0xCBF29CE484222325
    p = 0x100000001B3
    for b in s.encode("utf-8"):
        x ^= b
        x = (x * p) & ((1 << 64) - 1)
    return f"{x:016x}"

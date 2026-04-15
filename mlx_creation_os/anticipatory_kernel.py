#!/usr/bin/env python3
"""Shadow-aware anticipation: skeleton proof_receipt before prompt is final. 1 = 1"""
from __future__ import annotations

import json
import os
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List

from creation_os_core import query_hash_key

_ROOT = Path(__file__).resolve().parent


def _shadow_tail(path: Path, max_lines: int = 400) -> List[Dict[str, Any]]:
    if not path.is_file():
        return []
    lines = path.read_text(encoding="utf-8").splitlines()
    out: List[Dict[str, Any]] = []
    for line in lines[-max_lines:]:
        line = line.strip()
        if not line:
            continue
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return out


def shadow_topology_hints(partial_prompt: str, *, shadow_path: Path | None = None) -> Dict[str, Any]:
    """Recent shadow entries whose prompt shares a word-prefix with partial input."""
    p = shadow_path or Path(os.environ.get("SPEKTRE_SHADOW_LEDGER", str(_ROOT / "shadow_ledger.jsonl")))
    tail = _shadow_tail(p)
    stub = partial_prompt.strip().lower()
    if len(stub) < 2:
        return {"matches": [], "partial_key": query_hash_key(partial_prompt)}
    words = set(stub.replace("\n", " ").split())
    matches: List[Dict[str, Any]] = []
    for e in tail:
        prev = str(e.get("prompt") or "").lower()
        if not prev:
            continue
        if any(w in prev for w in words if len(w) > 2):
            matches.append(
                {
                    "reason": e.get("reason"),
                    "tier": e.get("tier"),
                    "prompt_key": query_hash_key(str(e.get("prompt") or "")),
                }
            )
    return {"matches": matches[-8:], "partial_key": query_hash_key(partial_prompt)}


def anticipate_proof_receipt(partial_prompt: str) -> Dict[str, Any]:
    """Emit a proof-shaped JSON shell before generation (anticipatory / streaming UIs)."""
    topo = shadow_topology_hints(partial_prompt)
    now = datetime.now(timezone.utc).isoformat()
    return {
        "anticipatory": True,
        "anticipatory_kernel": "v1",
        "timestamp": now,
        "partial_prompt": partial_prompt,
        "partial_key": topo["partial_key"],
        "shadow_topology": topo,
        "cases": [],
        "audit_status": "anticipatory_skeleton",
        "signature": "1=1 | Designed by Lauri Elias Rainio | Helsinki",
    }

"""
In-memory audit ring for σ-gate MCP tool calls (operator retention / governance).

This is a **logging hook** for your own compliance and retention policies. It is
**not** a legal certification of EU AI Act or any other regulatory regime.
"""
from __future__ import annotations

import threading
import time
from collections import deque
from typing import Any, Dict, List

_MAX = 10_000
_lock = threading.Lock()
_log: deque[Dict[str, Any]] = deque(maxlen=_MAX)


def append_record(record: Dict[str, Any]) -> None:
    """Append one JSON-serializable record (adds ``ts_unix`` if missing)."""
    r = dict(record)
    if "ts_unix" not in r:
        r["ts_unix"] = time.time()
    with _lock:
        _log.append(r)


def tail(last_n: int) -> List[Dict[str, Any]]:
    n = max(0, int(last_n))
    with _lock:
        return list(_log)[-n:] if n else []


def stats() -> Dict[str, Any]:
    with _lock:
        total = len(_log)
        if total == 0:
            return {"total_verifications": 0}
        accepts = sum(1 for r in _log if r.get("decision") == "ACCEPT")
        abstains = sum(1 for r in _log if r.get("decision") == "ABSTAIN")
        sigmas = [float(r["sigma"]) for r in _log if isinstance(r.get("sigma"), (int, float))]
        avg_s = sum(sigmas) / len(sigmas) if sigmas else 0.0
        return {
            "total_verifications": total,
            "accept_rate": accepts / total,
            "abstain_rate": abstains / total,
            "avg_sigma": avg_s,
        }


def clear() -> None:
    """Test-only reset."""
    with _lock:
        _log.clear()

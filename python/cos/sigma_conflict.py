# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-conflict: multi-node disagreement resolution using calibrated σ as the primary signal.

σ **measures** epistemic strain; it is not a popularity vote. Strategies combine measurements
with simple structural rules (weighted agreement mass, triangulation quorum). Proconductor
(Lauri) remains the final human override path — ``sigma_gate.h`` is untouched.

See ``docs/CLAIM_DISCIPLINE.md`` — lab conflict JSON is not harness AUROC.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Mapping, MutableSequence, Optional, Sequence


def _as_text(value: Any, *, max_len: int = 10_000) -> str:
    s = str(value) if value is not None else ""
    return s[:max_len]


def normalize_node_row(raw: Mapping[str, Any]) -> Dict[str, Any]:
    """Map legacy ``node`` / ``result`` keys to v166 ``node_id`` / ``response``."""
    nid = raw.get("node_id")
    if nid is None:
        nid = raw.get("node") or "unknown"
    resp = raw.get("response")
    if resp is None:
        resp = raw.get("result") or ""
    sigma = float(raw.get("sigma", 0.5))
    verdict = str(raw.get("verdict", "ACCEPT") or "ACCEPT")
    return {"node_id": str(nid), "response": _as_text(resp), "sigma": sigma, "verdict": verdict}


class SigmaConflict:
    """Resolve disagreeing node outputs using σ-shaped strategies (lab / audit)."""

    STRATEGIES: Sequence[str] = ("sigma_wins", "weighted_vote", "triangulation", "proconductor")

    def __init__(self, gate: Any = None, strategy: str = "sigma_wins") -> None:
        self.gate = gate
        self.strategy = str(strategy).strip()
        if self.strategy not in self.STRATEGIES:
            self.strategy = "sigma_wins"
        self.history: List[Dict[str, Any]] = []

    def resolve(self, node_responses: Sequence[Mapping[str, Any]]) -> Optional[Dict[str, Any]]:
        """Return winning row dict, a proconductor bundle, or ``None`` when input empty."""
        rows = [normalize_node_row(r) for r in node_responses]
        if not rows:
            return None
        if len(rows) == 1:
            return rows[0]

        if not self.is_conflict(rows):
            return min(rows, key=lambda r: float(r["sigma"]))

        if self.strategy == "sigma_wins":
            result = self.resolve_sigma_wins(rows)
        elif self.strategy == "weighted_vote":
            result = self.resolve_weighted_vote(rows)
        elif self.strategy == "triangulation":
            result = self.resolve_triangulation(rows)
        elif self.strategy == "proconductor":
            result = self.resolve_proconductor(rows)
        else:
            result = self.resolve_sigma_wins(rows)

        winner_id: str
        if isinstance(result, dict) and result.get("requires_human"):
            winner_id = str(result.get("node_id", "proconductor"))
        else:
            assert isinstance(result, dict)
            winner_id = str(result["node_id"])

        self.history.append(
            {
                "nodes": len(rows),
                "strategy": self.strategy,
                "winner": winner_id,
                "conflict_type": self.classify_conflict(rows),
            }
        )
        return result

    def is_conflict(self, responses: Sequence[Mapping[str, Any]]) -> bool:
        """True if surface answers diverge or σ spread is large (lab heuristic)."""
        texts = {_as_text(r.get("response", ""))[:100] for r in responses}
        if len(texts) > 1:
            return True
        sigmas = [float(r["sigma"]) for r in responses]
        if max(sigmas) - min(sigmas) > 0.3:
            return True
        return False

    def classify_conflict(self, responses: Sequence[Mapping[str, Any]]) -> str:
        verdicts = {str(r.get("verdict", "")) for r in responses}
        if "ACCEPT" in verdicts and "ABSTAIN" in verdicts:
            return "accept_vs_abstain"
        if len({_as_text(r.get("response", ""))[:50] for r in responses}) > 1:
            return "different_answers"
        return "sigma_disagreement"

    def resolve_sigma_wins(self, responses: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        rows = [normalize_node_row(r) for r in responses]
        return min(rows, key=lambda r: float(r["sigma"]))

    def resolve_weighted_vote(self, responses: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        rows = [normalize_node_row(r) for r in responses]
        groups: Dict[str, List[Dict[str, Any]]] = {}
        for r in rows:
            key = _as_text(r["response"])[:100]
            groups.setdefault(key, []).append(r)
        group_scores: Dict[str, Dict[str, Any]] = {}
        for key, members in groups.items():
            score = sum(1.0 / (float(m["sigma"]) + 0.01) for m in members)
            group_scores[key] = {
                "score": score,
                "best": min(members, key=lambda m: float(m["sigma"])),
                "supporters": len(members),
            }
        winner_key = max(group_scores, key=lambda k: float(group_scores[k]["score"]))
        return group_scores[winner_key]["best"]

    def resolve_triangulation(self, responses: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        rows = [normalize_node_row(r) for r in responses]
        n = len(rows)
        groups: Dict[str, List[Dict[str, Any]]] = {}
        for r in rows:
            key = _as_text(r["response"])[:100]
            groups.setdefault(key, []).append(r)
        need = (2 * n) / 3.0
        for _key, members in groups.items():
            if len(members) >= need:
                avg_sigma = sum(float(m["sigma"]) for m in members) / len(members)
                if avg_sigma < 0.3:
                    return min(members, key=lambda m: float(m["sigma"]))
        return self.resolve_sigma_wins(rows)

    def resolve_proconductor(self, responses: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        return {
            "node_id": "proconductor",
            "response": None,
            "sigma": None,
            "verdict": "PENDING",
            "options": [normalize_node_row(r) for r in responses],
            "requires_human": True,
        }


class SigmaConflictResolver:
    """
    Thin adapter for ``cos resolve`` (legacy ``node`` / ``result`` rows) and audit JSON.

    ``resolve`` returns a small bundle suitable for kernel lock metadata (``winner`` string).
    """

    def __init__(self, strategy: str = "sigma_wins", gate: Any = None) -> None:
        self._sc = SigmaConflict(gate, strategy=strategy)

    def resolve(self, results: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        rows = [normalize_node_row(r) for r in results]
        out = self._sc.resolve(rows)
        if out is None:
            return {"winner": None, "strategy": self._sc.strategy, "resolution": None}
        if out.get("requires_human"):
            return {
                "winner": "proconductor",
                "strategy": self._sc.strategy,
                "resolution": out,
                "verdict": out.get("verdict"),
            }
        return {
            "winner": out["node_id"],
            "sigma": out["sigma"],
            "strategy": self._sc.strategy,
            "resolution": out,
            "verdict": out.get("verdict"),
        }


# --- optional JSON history (CLI / demos) ---

DEFAULT_HISTORY_PATH = Path("~/.cos/sigma_conflict_history.json").expanduser()


def append_conflict_history(
    record: Mapping[str, Any],
    path: Optional[Path] = None,
) -> None:
    p = path or DEFAULT_HISTORY_PATH
    p.parent.mkdir(parents=True, exist_ok=True)
    bucket: MutableSequence[Dict[str, Any]]
    if p.is_file():
        try:
            raw = json.loads(p.read_text(encoding="utf-8"))
            bucket = raw if isinstance(raw, list) else []
        except json.JSONDecodeError:
            bucket = []
    else:
        bucket = []
    bucket.append(dict(record))
    p.write_text(json.dumps(bucket, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def summarize_conflict_history(path: Optional[Path] = None) -> Dict[str, Any]:
    p = path or DEFAULT_HISTORY_PATH
    if not p.is_file():
        return {"path": str(p), "total": 0, "by_strategy": {}, "by_conflict_type": {}}
    try:
        raw = json.loads(p.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return {"path": str(p), "total": 0, "by_strategy": {}, "by_conflict_type": {}, "error": "invalid_json"}
    if not isinstance(raw, list):
        return {"path": str(p), "total": 0, "by_strategy": {}, "by_conflict_type": {}, "error": "not_a_list"}
    from collections import Counter

    strat = Counter(str(x.get("strategy", "")) for x in raw if isinstance(x, dict))
    ctype = Counter(str(x.get("conflict_type", "")) for x in raw if isinstance(x, dict))
    return {
        "path": str(p),
        "total": len(raw),
        "by_strategy": dict(strat),
        "by_conflict_type": dict(ctype),
    }


__all__ = [
    "DEFAULT_HISTORY_PATH",
    "SigmaConflict",
    "SigmaConflictResolver",
    "append_conflict_history",
    "normalize_node_row",
    "summarize_conflict_history",
]

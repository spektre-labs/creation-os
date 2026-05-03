# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-trajectory — track bench metrics across ``_summary_*.json`` receipts (mtime order).

For claim hygiene, treat receipts as **lab evidence paths**; see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List


class SigmaTrajectory:
    """Load σ-bench summary history and compute simple trends."""

    def __init__(self, receipts_dir: str | Path = "benchmarks/results") -> None:
        self.receipts_dir = Path(receipts_dir)

    def load_history(self) -> List[Dict[str, Any]]:
        paths = sorted(self.receipts_dir.glob("_summary_*.json"), key=lambda p: p.stat().st_mtime)
        out: List[Dict[str, Any]] = []
        for p in paths:
            try:
                out.append(json.loads(p.read_text(encoding="utf-8")))
            except (OSError, json.JSONDecodeError):
                continue
        return out

    def trajectory(self, metric: str = "sigma_mean", suite: str = "truthfulqa") -> List[Dict[str, Any]]:
        history = self.load_history()
        points: List[Dict[str, Any]] = []
        for h in history:
            ts = h.get("timestamp")
            res = h.get("results", {})
            if not isinstance(res, dict) or suite not in res:
                continue
            row = res.get(suite)
            if not isinstance(row, dict):
                continue
            val = row.get(metric)
            if val is None:
                continue
            points.append({"timestamp": ts, "value": float(val), "suite": suite, "metric": metric})
        return points

    def is_improving(self, suite: str = "truthfulqa", window: int = 5) -> str:
        points = self.trajectory("sigma_mean", suite)
        w = int(window)
        if len(points) < w:
            return "insufficient_data"
        recent = [float(p["value"]) for p in points[-w:]]
        trend = recent[-1] - recent[0]
        if trend < -0.01:
            return "improving"
        if trend > 0.01:
            return "degrading"
        return "stable"


__all__ = ["SigmaTrajectory"]

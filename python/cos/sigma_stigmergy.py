# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Stigmergy table (signal strengths keyed by environment coordinates) for ``cos swarm`` lab."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional


class SigmaStigmergy:
    """In-memory signal map with optional JSON persistence."""

    def __init__(self, persist_path: Optional[Path] = None) -> None:
        self.persist_path = Path(persist_path).expanduser() if persist_path else None
        self.environment: Dict[str, Dict[str, Any]] = {}
        self._load()

    def _load(self) -> None:
        if not self.persist_path or not self.persist_path.is_file():
            return
        try:
            raw = json.loads(self.persist_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            return
        if isinstance(raw, dict) and isinstance(raw.get("environment"), dict):
            self.environment = {str(k): dict(v) for k, v in raw["environment"].items()}

    def _save(self) -> None:
        if not self.persist_path:
            return
        self.persist_path.parent.mkdir(parents=True, exist_ok=True)
        self.persist_path.write_text(
            json.dumps({"environment": self.environment}, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )

    def deposit(self, key: str, strength: float, meta: Optional[Dict[str, Any]] = None) -> None:
        self.environment[str(key)] = {"strength": float(strength), "meta": dict(meta or {})}
        self._save()

    def strongest_signals(self, prefix: Optional[str], n: int) -> List[Dict[str, Any]]:
        items = sorted(
            self.environment.items(),
            key=lambda kv: float(kv[1].get("strength", 0.0)),
            reverse=True,
        )
        if prefix:
            p = str(prefix)
            items = [kv for kv in items if str(kv[0]).startswith(p)]
        out: List[Dict[str, Any]] = []
        for k, v in items[: max(1, int(n))]:
            out.append({"key": k, **v})
        return out

    def decay(self, rate: float) -> int:
        r = float(rate)
        removed = 0
        for k in list(self.environment.keys()):
            self.environment[k]["strength"] = float(self.environment[k].get("strength", 0.0)) - r
            if self.environment[k]["strength"] <= 0.0:
                del self.environment[k]
                removed += 1
        self._save()
        return removed


__all__ = ["SigmaStigmergy"]

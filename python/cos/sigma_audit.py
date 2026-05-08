# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Append-only JSONL audit helper for σ-agent and reports (local-first)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
from pathlib import Path
from typing import Any, Dict, Iterator


class SigmaAudit:
    """JSONL audit under ``path`` (``agent.jsonl`` + optional ``firewall.jsonl``)."""

    def __init__(self, path: str) -> None:
        self.root = Path(path).expanduser()
        self.root.mkdir(parents=True, exist_ok=True)
        self.agent_log = self.root / "agent.jsonl"
        self.firewall_log = self.root / "firewall.jsonl"

    def log_agent_step(self, row: Dict[str, Any]) -> None:
        self._append(self.agent_log, row)

    def log_firewall(self, row: Dict[str, Any]) -> None:
        self._append(self.firewall_log, row)

    @staticmethod
    def _append(p: Path, row: Dict[str, Any]) -> None:
        with p.open("a", encoding="utf-8") as f:
            f.write(json.dumps(row, ensure_ascii=False, default=str) + "\n")

    def iter_recent_lines(self, *, max_lines: int = 80) -> Iterator[str]:
        if not self.agent_log.is_file():
            return
        lines = self.agent_log.read_text(encoding="utf-8").splitlines()
        for line in lines[-max(0, int(max_lines)) :]:
            if line.strip():
                yield line

    def firewall_stats(self, max_lines: int = 4000) -> Dict[str, Any]:
        if not self.firewall_log.is_file():
            return {"lines": 0, "blocked": 0}
        lines = self.firewall_log.read_text(encoding="utf-8").splitlines()[-max_lines:]
        blocked = 0
        for line in lines:
            try:
                o = json.loads(line)
            except json.JSONDecodeError:
                continue
            if bool(o.get("blocked")):
                blocked += 1
        return {"lines": len(lines), "blocked": blocked, "path": str(self.firewall_log)}


__all__ = ["SigmaAudit"]

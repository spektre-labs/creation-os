# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Local JSON registry for **σ-MCP** server entries (lab; no network).

Persisted as a single JSON object: ``{"servers": {id: {...}}}``.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, Iterator, Optional


class SigmaMCPRegistry:
    def __init__(self, servers: Optional[Dict[str, Dict[str, Any]]] = None) -> None:
        self.servers: Dict[str, Dict[str, Any]] = dict(servers or {})

    @classmethod
    def load(cls, path: Path) -> "SigmaMCPRegistry":
        if not path.is_file():
            return cls()
        try:
            raw = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return cls()
        if not isinstance(raw, dict):
            return cls()
        srv = raw.get("servers")
        if not isinstance(srv, dict):
            return cls()
        clean: Dict[str, Dict[str, Any]] = {}
        for k, v in srv.items():
            if isinstance(v, dict):
                clean[str(k)] = dict(v)
        return cls(clean)

    def register(self, server_id: str, server_url: str, metadata: Dict[str, Any]) -> None:
        sid = str(server_id).strip()
        self.servers[sid] = {"url": str(server_url), "metadata": dict(metadata)}

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps({"servers": self.servers}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def iter_lines(self) -> Iterator[str]:
        for sid, row in sorted(self.servers.items()):
            line = {"id": sid, **row}
            line["trust"] = "lab"
            line["calls"] = 0
            line["abstain_rate"] = 0.0
            yield json.dumps(line, ensure_ascii=False)


__all__ = ["SigmaMCPRegistry"]

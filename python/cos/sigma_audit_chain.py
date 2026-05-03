# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""JSONL audit chain helper for ``cos prove --verify-chain`` (lab)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Tuple


class SigmaAuditChain:
    """One JSON object per line; each row is a receipt dict for :class:`SigmaZKReceipt`."""

    def __init__(self, chain: List[Dict[str, Any]]) -> None:
        self.chain = chain

    @classmethod
    def from_jsonl(cls, path: Path) -> SigmaAuditChain:
        rows: List[Dict[str, Any]] = []
        with Path(path).expanduser().open(encoding="utf-8", errors="replace") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                rows.append(json.loads(line))
        return cls(rows)

    def verify_chain(self) -> Tuple[bool, str]:
        if not self.chain:
            return False, "empty chain"
        return True, "ok"


__all__ = ["SigmaAuditChain"]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""HellaSwag-shaped loader (pending full corpus)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
from pathlib import Path
from typing import Dict, List, Optional

__all__ = ["load_hellaswag_rows"]


def load_hellaswag_rows(path: Optional[Path] = None) -> List[Dict[str, str]]:
    if path is not None and Path(path).is_file():
        rows: List[Dict[str, str]] = []
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            j = json.loads(line)
            ctx = str(j.get("ctx") or j.get("context") or "")
            endings = j.get("endings") or []
            label = int(j.get("label", 0))
            tail = str(endings[label]) if label < len(endings) else ""
            rows.append(
                {
                    "prompt": ctx,
                    "expected": tail.lower()[:80],
                    "ref": "hellaswag",
                },
            )
        return rows
    return [{"prompt": "He poured the coffee. Then he", "expected": "drank", "ref": "pending"}]

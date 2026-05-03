# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""MMLU-shaped loader (pending full dataset wiring)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List, Optional

__all__ = ["load_mmlu_rows"]


def load_mmlu_rows(path: Optional[Path] = None) -> List[Dict[str, str]]:
    if path is not None and Path(path).is_file():
        rows: List[Dict[str, str]] = []
        for line in Path(path).read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            j = json.loads(line)
            q = str(j.get("question", ""))
            ans = str(j.get("answer", ""))
            rows.append({"prompt": q, "expected": ans.lower(), "ref": "mmlu"})
        return rows
    return [{"prompt": "Sample MMLU stem (A-D)?", "expected": "a", "ref": "pending"}]

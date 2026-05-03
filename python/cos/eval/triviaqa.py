# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""TriviaQA-shaped loader (lab)."""
from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, List, Optional

__all__ = ["load_triviaqa_rows"]


def load_triviaqa_rows(path: Optional[Path] = None) -> List[Dict[str, str]]:
    if path is not None and Path(path).is_file():
        raw = json.loads(Path(path).read_text(encoding="utf-8"))
        if isinstance(raw, list):
            rows_src = raw
        else:
            rows_src = raw.get("data", [])
        out: List[Dict[str, str]] = []
        for j in rows_src:
            q = str(j.get("question") or "")
            a = str(j.get("answer") or j.get("value") or "")
            out.append({"prompt": q, "expected": a.lower()[:80], "ref": "triviaqa"})
        return out
    return [
        {"prompt": "Capital of France?", "expected": "paris", "ref": "demo"},
    ]

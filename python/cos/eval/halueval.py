# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""HaluEval-shaped rows with oracle-alignment checks (lab).

“Oracle-pair bug”: domain-shift + wrong train/test pairing makes probe metrics look broken;
fix by **pairing** ``prompt``, ``label``, ``knowledge`` consistently and calibrating on-target.
"""
from __future__ import annotations

import json
from pathlib import Path
from typing import Any, Dict, List, Optional

__all__ = ["fix_oracle_pairs", "load_halueval_rows"]


def fix_oracle_pairs(rows: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Drop rows with missing question/label alignment; normalize keys for σ-bench."""
    fixed: List[Dict[str, Any]] = []
    for raw in rows:
        q = str(raw.get("question") or raw.get("prompt") or "").strip()
        if not q:
            continue
        label = raw.get("label")
        if label is None:
            label = raw.get("is_hallucination")
        lh = str(raw.get("l") or raw.get("category") or "").strip()
        hallu = str(label).lower() in ("1", "true", "hallucination", "yes")
        expected = "hallucination" if hallu else "aligned"
        fixed.append(
            {
                "prompt": q,
                "expected": expected,
                "ref": lh or "halueval",
            },
        )
    return fixed


def load_halueval_rows(path: Optional[Path] = None) -> List[Dict[str, str]]:
    """Load JSONL / JSON list; falls back to two demo rows (offline)."""
    if path is not None and Path(path).is_file():
        text = Path(path).read_text(encoding="utf-8")
        if path.suffix.lower() == ".jsonl":
            rows = [json.loads(line) for line in text.splitlines() if line.strip()]
        else:
            rows = json.loads(text)
            if isinstance(rows, dict) and "data" in rows:
                rows = rows["data"]
        return fix_oracle_pairs(list(rows))
    return fix_oracle_pairs(
        [
            {
                "question": "The sky is green during clear daylight.",
                "label": 1,
                "l": "color",
            },
            {
                "question": "Birds have feathers.",
                "label": 0,
                "l": "biology",
            },
        ],
    )

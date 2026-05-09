# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Smoke tests for :mod:`cos.eval.gemma_eval` discrimination lab (mock path)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.eval.gemma_eval import GPQA_SAMPLE, GemmaEval  # noqa: E402


def test_gpqa_sample_has_thirty() -> None:
    assert len(GPQA_SAMPLE) == 30


def test_gemma_eval_mock_run() -> None:
    g = GemmaEval(mock=True, model="test-model")
    r = g.run(n=3)
    assert r["n"] == 3
    assert r["mock_mode"] is True
    assert "hallucination_catch_rate" in r
    assert len(g.results) == 3


def test_gemma_eval_save_writes(tmp_path: Path) -> None:
    g = GemmaEval(mock=True)
    g.run(n=2)
    p = tmp_path / "out.json"
    g.save(p)
    assert p.is_file()
    text = p.read_text(encoding="utf-8")
    assert "summary" in text
    assert "results" in text

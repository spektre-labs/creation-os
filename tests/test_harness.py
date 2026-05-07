# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

import pytest

from cos.bench import HALUEVAL_EXAMPLE_AUROC
from cos.eval.harness import SigmaHarness, default_harness_dataset


class _FixedGate:
    def __init__(self, pairs: list[tuple[float, str]]) -> None:
        self._pairs = list(pairs)
        self._i = 0

    def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
        s, v = self._pairs[min(self._i, len(self._pairs) - 1)]
        self._i += 1
        return s, v


def test_run_returns_metrics(tmp_path: Path) -> None:
    g = _FixedGate([(0.1, "ACCEPT")] * 10)
    h = SigmaHarness(gate=g, output_dir=tmp_path)
    ds = [("p", "r", True), ("p2", "bad", False)]
    m = h.run(ds, name="unit", n=2, checkpoint_every=5)
    assert m.get("n") == 2
    assert "accuracy" in m
    assert "auroc" in m
    assert "abstention_rate" in m
    assert "smece" in m


def test_auroc_perfect_separation(tmp_path: Path) -> None:
    h = SigmaHarness(gate=_FixedGate([(0.0, "ACCEPT")]), output_dir=tmp_path)
    rows = [
        {"i": 0, "σ": 0.1, "verdict": "ACCEPT", "gold": True, "correct_detection": True},
        {"i": 1, "σ": 0.9, "verdict": "RETHINK", "gold": False, "correct_detection": True},
    ]
    m = h._compute_metrics(rows, "auc_good")
    assert m["auroc"] == 1.0


def test_auroc_random(tmp_path: Path) -> None:
    h = SigmaHarness(gate=_FixedGate([(0.0, "ACCEPT")]), output_dir=tmp_path)
    rows = [
        {"i": i, "σ": 0.5, "verdict": "ACCEPT", "gold": i % 2 == 0, "correct_detection": True}
        for i in range(8)
    ]
    m = h._compute_metrics(rows, "auc_flat")
    assert abs(float(m["auroc"]) - 0.5) < 1e-6


def test_checkpoint_saves(tmp_path: Path) -> None:
    g = _FixedGate([(0.2, "ACCEPT")] * 10)
    h = SigmaHarness(gate=g, output_dir=tmp_path)
    ds = default_harness_dataset("TruthfulQA")[:3]
    h.run(ds, name="ck", n=3, checkpoint_every=5)
    ck = tmp_path / "checkpoint_ck.jsonl"
    assert ck.is_file()
    lines = [ln for ln in ck.read_text(encoding="utf-8").splitlines() if ln.strip()]
    assert len(lines) == 3


def test_checkpoint_resumes(tmp_path: Path) -> None:
    ck = tmp_path / "checkpoint_resume.jsonl"
    for i in range(2):
        row = {"i": i, "σ": 0.2, "verdict": "ACCEPT", "gold": True, "correct_detection": True}
        with ck.open("a", encoding="utf-8") as f:
            f.write(json.dumps(row, ensure_ascii=False) + "\n")

    g = _FixedGate([(0.3, "ACCEPT"), (0.4, "ACCEPT")])
    h = SigmaHarness(gate=g, output_dir=tmp_path)
    ds = default_harness_dataset("TruthfulQA")[:4]
    h.run(ds, name="resume", n=4, checkpoint_every=5)
    lines = [ln for ln in ck.read_text(encoding="utf-8").splitlines() if ln.strip()]
    assert len(lines) == 4


def test_mtier_table_format(capsys: pytest.CaptureFixture[str]) -> None:
    h = SigmaHarness(output_dir=Path("."))
    h.mtier_table(
        [
            {
                "name": "TruthfulQA lab",
                "auroc": 0.72,
                "abstention_rate": 0.1,
                "smece": 0.04,
                "status": "⚠",
            }
        ]
    )
    out = capsys.readouterr().out
    assert "Benchmark" in out
    assert str(HALUEVAL_EXAMPLE_AUROC) in out or "0.514" in out
    assert "HaluEval" in out
    assert "saturated" in out.lower()


def test_smece_calculation(tmp_path: Path) -> None:
    h = SigmaHarness(output_dir=tmp_path)
    v = h._smece([0.05, 0.95], [True, False])
    assert abs(v - 0.05) < 1e-9

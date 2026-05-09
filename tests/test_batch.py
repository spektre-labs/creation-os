# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.batch` (process-pool σ scoring)."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

from cos.batch import SigmaBatch, score_one_pair

_REPO = Path(__file__).resolve().parents[1]


def test_score_one_pair_smoke() -> None:
    row = score_one_pair(("What is 2+2?", "4", 0))
    assert row["idx"] == 0
    assert "sigma" in row
    assert row.get("verdict") in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_sigma_batch_score_pairs_parallel() -> None:
    pairs = [
        ("What is 2+2?", "4"),
        ("What is 2+2?", "banana"),
        ("Capital of France?", "Paris"),
    ]
    out = SigmaBatch(workers=2).score_pairs(pairs)
    assert out["total"] == 3
    assert len(out["results"]) == 3
    assert out["results"][0]["idx"] == 0
    assert out["throughput"] >= 0
    assert "avg_sigma" in out


def test_sigma_batch_jsonl_roundtrip(tmp_path: Path) -> None:
    src = tmp_path / "in.jsonl"
    dst = tmp_path / "out.jsonl"
    src.write_text(
        '{"prompt": "p1", "response": "r1"}\n{"prompt": "p2", "response": "r2"}\n',
        encoding="utf-8",
    )
    summary = SigmaBatch(workers=2).score_jsonl(src, dst)
    assert summary["total"] == 2
    lines = dst.read_text(encoding="utf-8").strip().splitlines()
    assert len(lines) == 2
    row = json.loads(lines[0])
    assert "sigma" in row and "verdict" in row


def _env() -> dict[str, str]:
    import os

    env = os.environ.copy()
    py = str(_REPO / "python")
    env["PYTHONPATH"] = py + os.pathsep + env.get("PYTHONPATH", "")
    return env


def test_cli_batch_json(tmp_path: Path) -> None:
    src = tmp_path / "data.jsonl"
    src.write_text('{"prompt": "x", "response": "y"}\n', encoding="utf-8")
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "batch",
            "--input",
            str(src),
            "--json",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    data = json.loads((result.stdout or "").strip())
    assert data["total"] == 1
    assert "avg_sigma" in data

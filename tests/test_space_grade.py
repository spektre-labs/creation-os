# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.space_grade import SpaceGradeChecker  # noqa: E402


def test_check_file_compliant(tmp_path: Path) -> None:
    f = tmp_path / "ok.py"
    f.write_text("def f():\n    return 1\n", encoding="utf-8")
    r = SpaceGradeChecker().check_file(f)
    assert r["compliant"] is True
    assert r["violations"] == []


def test_check_file_long_function(tmp_path: Path) -> None:
    body = "\n".join(["    x = 1"] * 65)
    f = tmp_path / "long.py"
    f.write_text(f"def big():\n{body}\n", encoding="utf-8")
    r = SpaceGradeChecker().check_file(f)
    assert r["compliant"] is False
    assert any(v["rule"] == 4 for v in r["violations"])


def test_check_file_unbounded_loop(tmp_path: Path) -> None:
    f = tmp_path / "loop.py"
    f.write_text(
        "def g():\n"
        "    i = 0\n"
        "    while i < 10:\n"
        "        i += 1\n",
        encoding="utf-8",
    )
    r = SpaceGradeChecker().check_file(f)
    assert r["compliant"] is False
    assert any(v["rule"] == 2 for v in r["violations"])


def test_tmr_majority_vote() -> None:
    C = SpaceGradeChecker()
    out = C.tmr_check([1], [1], [1])
    assert out["voted_results"][0]["value"] == 1
    assert out["voted_results"][0]["unanimous"] is True
    assert out["corrections"] == 0


def test_tmr_correction_detected() -> None:
    C = SpaceGradeChecker()
    out = C.tmr_check(["a", "x"], ["a", "y"], ["b", "y"])
    assert out["voted_results"][0]["corrected"] is True
    assert out["corrections"] >= 1

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.jepa import SigmaJEPA  # noqa: E402


def _len_vec(v: object) -> int:
    if hasattr(v, "shape"):
        return int(getattr(v, "shape")[0])  # type: ignore[index]
    return len(v)  # type: ignore[arg-type]


def test_encode_produces_vector() -> None:
    j = SigmaJEPA(dim=64)
    v = j.encode("observation-alpha")
    assert _len_vec(v) == 64


def test_encode_deterministic_same_input() -> None:
    j = SigmaJEPA(dim=32)
    a = j.encode("same")
    b = j.encode("same")
    if hasattr(a, "tolist"):
        a = a.tolist()  # type: ignore[assignment]
    if hasattr(b, "tolist"):
        b = b.tolist()  # type: ignore[assignment]
    assert list(a) == list(b)


def test_encode_different_for_different_input() -> None:
    j = SigmaJEPA(dim=48)
    a = j.encode("aaaa")
    b = j.encode("zzzz")
    if hasattr(a, "tolist"):
        a = a.tolist()  # type: ignore[assignment]
    if hasattr(b, "tolist"):
        b = b.tolist()  # type: ignore[assignment]
    assert list(a) != list(b)


def test_predict_returns_vector() -> None:
    j = SigmaJEPA(dim=16)
    j.step("first")
    pred = j.predict(j.states[-1])
    assert _len_vec(pred) == 16


def test_step_returns_sigma() -> None:
    j = SigmaJEPA(dim=24)
    r = j.step("only")
    assert "σ" in r and "verdict" in r
    assert r["σ"] == 0.0


def test_step_sequence_builds_history() -> None:
    j = SigmaJEPA(dim=20)
    j.step("t0")
    j.step("t1")
    assert len(j.states) == 2
    assert len(j.σ_history) == 2


def test_surprise_rate_calculation() -> None:
    j = SigmaJEPA(dim=8)
    for k in range(6):
        j.step(f"token-{k}-sequence-longer")
    sr = j.surprise_rate()
    assert 0.0 <= sr <= 1.0
    assert len(j.σ_history) == 6


def test_avg_sigma_calculation() -> None:
    j = SigmaJEPA(dim=8)
    j.step("one")
    assert j.avg_σ() == 0.0
    j.step("two")
    assert j.avg_σ() >= 0.0

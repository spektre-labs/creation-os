# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.distill import SigmaDistill
from cos.sigma_gate import SigmaGate


def test_distill_step_flags_high_sigma() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    batch = [
        {"prompt": "q", "student_text": "x" * 120},
    ]
    out = d.distill_step(None, None, batch, gate)
    assert out["batch_size"] == 1
    assert "full_kd" in out["rows"][0]


def test_selective_tokens() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    tlog = [1.0, 0.1, 2.0]
    slog = [0.2, 0.1, 0.0]
    sel = d.selective_tokens(tlog, slog, gate)
    assert isinstance(sel["need_help"], list)


def test_curriculum_order() -> None:
    d = SigmaDistill()
    order = d.sigma_curriculum([0.9, 0.1, 0.5])
    assert order[0] in (1,)


def test_efficiency() -> None:
    d = SigmaDistill()
    eff = d.efficiency({"kd_fraction": 0.2})
    assert eff["skip_fraction"] == 0.8


def test_empty_batch() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    out = d.distill_step(None, None, [], gate)
    assert out["batch_size"] == 0
    assert out["kd_fraction"] == 0.0


def test_gate_integration() -> None:
    d = SigmaDistill()
    gate = SigmaGate()
    out = d.distill_step(
        None,
        None,
        [{"prompt": "2+2", "student_text": "4"}],
        gate,
    )
    assert "sigma" in out["rows"][0]


def test_evaluate_pair_pass(monkeypatch: pytest.MonkeyPatch) -> None:
    d = SigmaDistill(tolerance=0.1)

    def fake_score(prompt, response, **_k):  # noqa: ANN001, ARG001
        if "teacher_marker" in str(response):
            return 0.2, "ACCEPT"
        return 0.25, "ACCEPT"

    monkeypatch.setattr(d.gate, "score", fake_score)
    r = d.evaluate_pair("p", "teacher_marker", "student_marker")
    assert r["acceptable"] is True
    assert r["verdict"] == "PASS"
    assert r["gap"] == pytest.approx(0.05)


def test_evaluate_pair_fail(monkeypatch: pytest.MonkeyPatch) -> None:
    d = SigmaDistill(tolerance=0.05)

    def fake_score(prompt, response, **_k):  # noqa: ANN001, ARG001
        if "T" in str(response):
            return 0.1, "ACCEPT"
        return 0.8, "ABSTAIN"

    monkeypatch.setattr(d.gate, "score", fake_score)
    r = d.evaluate_pair("q", "T", "S")
    assert r["acceptable"] is False
    assert r["verdict"] == "FAIL"


def test_validate_all_pass(monkeypatch: pytest.MonkeyPatch) -> None:
    d = SigmaDistill(tolerance=0.2)
    monkeypatch.setattr(d.gate, "score", lambda _p, _r, **_k: (0.3, "RETHINK"))
    data = [("a", "t0", "s0"), ("b", "t1", "s1")]
    out = d.validate(data)
    assert out["passed"] == 2
    assert out["verdict"] == "ACCEPT"


def test_validate_mixed(monkeypatch: pytest.MonkeyPatch) -> None:
    """One failing pair in a small set ⇒ aggregate verdict not ACCEPT."""
    d = SigmaDistill(tolerance=0.05)
    pairs = [
        (0.10, 0.11),
        (0.10, 0.50),
    ]
    call = {"k": 0}

    def fake_score(_p, response, **_k):  # noqa: ARG001
        teacher = "teacher" in str(response)
        i = call["k"] // 2
        call["k"] += 1
        σ_t, σ_s = pairs[i]
        return (σ_t if teacher else σ_s), "ACCEPT"

    monkeypatch.setattr(d.gate, "score", fake_score)
    data = [("p1", "t1_teacher", "s1_student"), ("p2", "t2_teacher", "s2_student")]
    out = d.validate(data)
    assert out["failed"] == 1
    assert out["verdict"] == "REJECT"


def test_worst_cases_sorted(monkeypatch: pytest.MonkeyPatch) -> None:
    d = SigmaDistill(tolerance=0.5)

    def fake_pair_score(prompt, response, **_k):  # noqa: ANN001, ARG001
        if str(prompt) == "easy":
            return 0.1, "ACCEPT" if "teacher" in str(response) else (0.12, "ACCEPT")
        if str(prompt) == "mid":
            return 0.1, "ACCEPT" if "teacher" in str(response) else (0.5, "RETHINK")
        return 0.05, "ACCEPT" if "teacher" in str(response) else (0.9, "ABSTAIN")

    monkeypatch.setattr(d.gate, "score", fake_pair_score)
    data = [
        ("easy", "teacher_e", "student_e"),
        ("mid", "teacher_m", "student_m"),
        ("bad", "teacher_b", "student_b"),
    ]
    out = d.validate(data)
    wc = out["worst_cases"]
    assert len(wc) <= 5
    assert wc[0]["gap"] >= wc[-1]["gap"]


def test_recommend_compression() -> None:
    d = SigmaDistill()
    r0 = d.recommend_compression(0.5, 0.45)
    assert r0["max_compression"] == "none"
    r1 = d.recommend_compression(0.2, 0.25)
    assert "quantization" in r1["recommendation"].lower()
    assert r1["expected_σ_increase"] == 0.02
    r2 = d.recommend_compression(0.2, 0.32)
    assert "distillation" in r2["recommendation"].lower()
    r3 = d.recommend_compression(0.1, 0.5)
    assert "prune" in r3["recommendation"].lower() or "aggressive" in r3["recommendation"].lower()

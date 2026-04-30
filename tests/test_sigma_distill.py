# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_distill import SigmaDistill, _stabilized_verdict  # noqa: E402
from cos.sigma_gate_core import Verdict  # noqa: E402


def test_stabilized_accept_for_low_sigma() -> None:
    v, _ = _stabilized_verdict(0.18, 0.95)
    assert v == Verdict.ACCEPT


def test_generate_training_data_accepts_only_clean() -> None:
    class T:
        def generate(self, prompt: str) -> str:
            return "ok" if "good" in prompt else "bad"

    class G:
        def compute_sigma(self, _teacher: object, prompt: str, output: str) -> float:
            if "good" in prompt:
                return 0.18
            return 0.95

    class S:
        def train_step(self, *a: object, **k: object) -> float:
            return 0.0

    sd = SigmaDistill(teacher=T(), student=S(), gate=G(), k_raw=0.95)
    rows = sd.generate_training_data(["good 1", "bad 1", "  ", "good 2"], limit=10)
    assert len(rows) == 2
    assert all("good" in r["prompt"] for r in rows)


def test_distill_and_weighted_count_steps() -> None:
    class T:
        pass

    class G:
        pass

    class S:
        def __init__(self) -> None:
            self.calls: list[tuple[float, ...]] = []

        def train_step(self, p: str, r: str, *, weight: float = 1.0) -> float:
            self.calls.append((weight,))
            return 0.0

    stu = S()
    sd = SigmaDistill(teacher=T(), student=stu, gate=G())
    data = [{"prompt": "a", "response": "b", "sigma": 0.5}]
    sd.distill(data, epochs=2)
    assert len(stu.calls) == 2
    stu2 = S()
    sd2 = SigmaDistill(teacher=T(), student=stu2, gate=G())
    sd2.distill_weighted(data, epochs=1)
    assert abs(stu2.calls[0][0] - 0.5) < 1e-9


def test_distill_residual_uses_ground_truth() -> None:
    class Tchr:
        def generate(self, prompt: str) -> str:
            return "teacher_out"

    class Gt:
        def compute_sigma(self, *_a: object, **_k: object) -> float:
            return 0.5

    class Stu:
        def __init__(self) -> None:
            self.seen: list[tuple[str, str, float]] = []

        def train_step(self, p: str, r: str, *, weight: float = 1.0) -> float:
            self.seen.append((p, r, weight))
            return 0.0

    stu = Stu()

    def gt(p: str) -> str | None:
        return "correct" if p == "p1" else None

    sd = SigmaDistill(
        teacher=Tchr(),
        student=stu,
        gate=Gt(),
        rethink_sigma_low=0.3,
        rethink_sigma_high=0.7,
        residual_weight=2.5,
        get_ground_truth=gt,
    )
    sd.distill_residual(["p1", "p2"])
    assert len(stu.seen) == 1
    assert stu.seen[0][0] == "p1" and stu.seen[0][1] == "correct" and abs(stu.seen[0][2] - 2.5) < 1e-9

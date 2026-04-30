# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_debate import SigmaDebate  # noqa: E402
from cos.sigma_proconductor import ProconductorDebate  # noqa: E402
from cos.sigma_selfplay import SigmaSelfPlay  # noqa: E402


def test_sigma_debate_picks_lower_mean_sigma() -> None:
    class A:
        def argue(self, question: str, hist: object) -> str:
            return "a-arg"

    class B:
        def argue(self, question: str, hist: object) -> str:
            return "b-arg"

    class G:
        def compute_sigma(self, model: object, _q: str, _arg: str) -> float:
            return 0.2 if isinstance(model, A) else 0.5

    d = SigmaDebate(A(), B(), G())
    text, side, stat = d.debate("q?", rounds=2)
    assert side == "A"
    assert text == "a-arg"
    assert stat == 0.2


def test_sigma_debate_tie() -> None:
    class M:
        def argue(self, _q: str, _h: object) -> str:
            return "x"

    class G:
        def compute_sigma(self, *_a: object) -> float:
            return 0.4

    d = SigmaDebate(M(), M(), G())
    text, side, stat = d.debate("q", rounds=1)
    assert side == "TIE" and text is None and abs(stat - 0.4) < 1e-9


def test_sigma_selfplay_prefers_stable_draw() -> None:
    class M:
        def generate(self, q: str, temperature: float = 0.5) -> str:
            if abs(temperature - 0.3) < 1e-6:
                return "lowtemp"
            return "hightemp"

        def critique(self, _q: str, answer: str) -> str:
            return f"crit-{answer}"

    class G:
        def compute_sigma(self, _m: object, _q: str, text: str) -> float:
            if "lowtemp" in text and not text.startswith("crit"):
                return 0.1
            if "hightemp" in text and not text.startswith("crit"):
                return 0.5
            if "crit-hightemp" in text:
                return 0.85
            if "crit-lowtemp" in text:
                return 0.15
            return 0.5

    sp = SigmaSelfPlay(G())
    ans, s = sp.play(M(), "why?")
    assert ans == "lowtemp"
    assert s == 0.1


def test_proconductor_consensus() -> None:
    class Md:
        def __init__(self, name: str, a: str) -> None:
            self.name = name
            self._a = a

        def generate(self, _q: str) -> str:
            return self._a

    class G:
        def compute_sigma(self, _m: object, _q: str, _a: str) -> float:
            return 0.15

    models = [Md("m1", "same"), Md("m2", "same"), Md("m3", "same"), Md("m4", "other")]
    pc = ProconductorDebate(models, G())
    ans, sig = pc.multi_debate("q")
    assert ans == "same" and sig == 0.15


def test_proconductor_abstain_without_consensus() -> None:
    class Md:
        def __init__(self, name: str) -> None:
            self.name = name

        def generate(self, _q: str) -> str:
            return f"ans-{self.name}"

    class G:
        def compute_sigma(self, *_a: object) -> float:
            return 0.3

    models = [Md("a"), Md("b"), Md("c"), Md("d")]
    pc = ProconductorDebate(models, G())
    ans, sig = pc.multi_debate("q")
    assert ans is None and sig is None

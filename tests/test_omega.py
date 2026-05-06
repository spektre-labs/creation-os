# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.config import SigmaConfig  # noqa: E402
from cos.graph import SigmaGraph  # noqa: E402
from cos.memory import SigmaMemory  # noqa: E402
from cos.omega import OmegaLoop  # noqa: E402
from cos.sigma_gate import SigmaGate  # noqa: E402


def _make_loop(gate, *, mem=None, gr=None):
    return OmegaLoop(
        gate=gate,
        memory=mem or SigmaMemory(gate=gate),
        graph=gr or SigmaGraph(gate=gate),
        config=SigmaConfig(),
    )


def test_single_step_returns_sigma() -> None:
    g = SigmaGate()
    loop = _make_loop(g)
    out = loop.step("hello world")
    assert "σ" in out and isinstance(out["σ"], float)
    assert out["step"] == 1


def test_abstain_on_high_sigma() -> None:
    class HiGate:
        threshold_accept = 0.15
        threshold_abstain = 0.85

        def score(self, p: str, r: str) -> tuple[float, str]:
            _ = (p, r)
            return 0.99, "ABSTAIN"

    loop = _make_loop(HiGate())
    out = loop.step("x")
    assert out["verdict"] == "ABSTAIN"
    assert out["result"]["kind"] == "abstain"


def test_rethink_retries() -> None:
    class RethinkGate:
        threshold_accept = 0.15
        threshold_abstain = 0.85

        def __init__(self) -> None:
            self._n = 0

        def score(self, p: str, r: str) -> tuple[float, str]:
            _ = (p, r)
            self._n += 1
            if self._n == 1:
                return 0.5, "RETHINK"
            return 0.05, "ACCEPT"

    loop = _make_loop(RethinkGate())
    out = loop.step("probe")
    assert out["verdict"] == "ACCEPT"
    assert out["result"]["kind"] == "accept"


def test_history_accumulates() -> None:
    loop = _make_loop(SigmaGate())
    loop.step("a")
    loop.step("b")
    assert len(loop.σ_history) == 2


def test_total_sigma_decreases_over_good_inputs() -> None:
    class CalmGate:
        threshold_accept = 0.5
        threshold_abstain = 0.99

        def __init__(self) -> None:
            self._i = 0

        def score(self, p: str, r: str) -> tuple[float, str]:
            _ = (p, r)
            self._i += 1
            s = 0.5 / float(self._i)
            return s, "ACCEPT"

    loop = _make_loop(CalmGate())
    loop.run(["i1", "i2", "i3"])
    t1 = loop.total_σ()
    loop2 = _make_loop(CalmGate())
    loop2.run(["i1", "i2", "i3", "i4"])
    t2 = loop2.total_σ()
    assert t2 < t1


def test_run_multiple_inputs() -> None:
    loop = _make_loop(SigmaGate())
    r = loop.run(["u", "v", "w"], max_steps=2)
    assert len(r) == 2
    assert r[0]["step"] == 1 and r[1]["step"] == 2


def test_reflect_produces_sigma_meta() -> None:
    loop = _make_loop(SigmaGate())
    out = loop.step("reflect meta check")
    assert "σ_meta" in out and isinstance(out["σ_meta"], float)

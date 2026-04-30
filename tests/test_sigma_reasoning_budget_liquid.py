# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_budget import Budget, SigmaBudget  # noqa: E402
from cos.sigma_gate_core import SigmaState, Verdict, sigma_update  # noqa: E402
from cos.sigma_liquid_model import SigmaLiquidModel  # noqa: E402
from cos.sigma_reasoning import SigmaReasoning  # noqa: E402


def test_sigma_reasoning_chain_then_accept() -> None:
    def gen_thought(p: str, t: tuple[str, ...]) -> str:
        return f"t{len(t)}"

    def compute_sigma(th: str) -> float:
        return 0.25

    def gen_answer(p: str, t: tuple[str, ...]) -> str:
        return "|".join(t) + "|a"

    r = SigmaReasoning(
        generate_thought=gen_thought,
        compute_sigma=compute_sigma,
        generate_answer=gen_answer,
        k_raw=0.95,
    )
    out = r.reason("p", max_depth=5)
    assert out == "t0|a"


def test_sigma_budget_tiers() -> None:
    b = SigmaBudget(precheck=lambda s: 0.05)
    assert b.allocate("x") == Budget(10, 0, "local_tiny")
    assert b.allocate("y", state=None) == Budget(10, 0, "local_tiny")
    assert SigmaBudget(precheck=lambda s: 0.2).allocate("z").tokens == 100
    assert SigmaBudget(precheck=lambda s: 0.5).allocate("z").tokens == 500
    assert SigmaBudget(precheck=lambda s: 0.7).allocate("z").tokens == 2000
    abst = SigmaBudget(precheck=lambda s: 0.9).allocate("z")
    assert abst.tokens == 0 and abst.action == "ABSTAIN"


def test_sigma_liquid_model_retry_then_accept() -> None:
    class M:
        def __init__(self) -> None:
            self.tau = 1.0

        def save_state(self) -> float:
            return self.tau

        def load_state(self, c: object) -> None:
            self.tau = float(c)

        def adjust_tau(self, *, factor: float) -> None:
            self.tau *= factor

        def __call__(self, x: int) -> int:
            return int(x) * 2

    calls = {"n": 0}

    def probe(o: object) -> float:
        calls["n"] += 1
        return 0.38 if calls["n"] == 1 else 0.18

    st = SigmaState()
    lm = SigmaLiquidModel(M(), probe=probe, k_raw=0.92)
    out, v = lm.forward(3, st)
    assert out == 6
    assert v == Verdict.ACCEPT


def test_sigma_liquid_model_warmed_accept_no_retry() -> None:
    class M:
        def save_state(self) -> None:
            return None

        def load_state(self, c: object) -> None:
            pass

        def adjust_tau(self, *, factor: float) -> None:
            pass

        def __call__(self, x: int) -> int:
            return x + 1

    st = SigmaState()
    sigma_update(st, 0.2, 0.95)
    sigma_update(st, 0.2, 0.95)
    lm = SigmaLiquidModel(M(), probe=lambda o: 0.2)
    out, v = lm.forward(4, st)
    assert out == 5 and v == Verdict.ACCEPT

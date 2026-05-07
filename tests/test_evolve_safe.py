# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.evolve import SigmaEvolve  # noqa: E402
from cos.sigma_gate import SigmaGate  # noqa: E402

_EVAL = [["What is 2+2?", "4"]]


def _worsen_gate(gg: SigmaGate) -> SigmaGate:
    ta = max(0.02, float(gg.threshold_accept) - 0.35)
    tb = float(gg.threshold_abstain)
    return SigmaGate(threshold_accept=ta, threshold_abstain=tb)


def _improve_gate(gg: SigmaGate, *, delta: float) -> SigmaGate:
    ta = min(0.78, float(gg.threshold_accept) + float(delta))
    tb = float(gg.threshold_abstain)
    if ta >= tb:
        ta = max(0.02, tb - 0.04)
    return SigmaGate(threshold_accept=ta, threshold_abstain=tb)


def test_risk_budget_halts() -> None:
    ev = SigmaEvolve()
    g = SigmaGate()
    out = ev.evolve_safe(g, _EVAL, max_steps=4, risk_budget=0.001, mutate_fn=_worsen_gate)
    assert any(h.get("action") == "HALT" for h in out["history"])
    assert "risk budget" in (out["history"][-1].get("reason") or "")


def _mild_worsen_gate(gg: SigmaGate) -> SigmaGate:
    ta = max(0.02, float(gg.threshold_accept) - 0.08)
    tb = float(gg.threshold_abstain)
    return SigmaGate(threshold_accept=ta, threshold_abstain=tb)


def test_goal_drift_rejects() -> None:
    def huge(gg: SigmaGate) -> SigmaGate:
        return _improve_gate(gg, delta=0.45)

    out = SigmaEvolve().evolve_safe(
        SigmaGate(),
        _EVAL,
        max_steps=2,
        risk_budget=2.0,
        mutate_fn=huge,
        drift_threshold=0.05,
    )
    assert any(h.get("action") == "REJECT" and h.get("reason") == "drift" for h in out["history"])


def test_regression_rejects() -> None:
    ev = SigmaEvolve()
    out = ev.evolve_safe(
        SigmaGate(),
        _EVAL,
        max_steps=2,
        risk_budget=2.0,
        mutate_fn=_mild_worsen_gate,
    )
    assert any(h.get("action") == "REJECT" and h.get("reason") == "regression" for h in out["history"])


def test_accept_when_utility_exceeds_risk() -> None:
    ev = SigmaEvolve()
    out = ev.evolve_safe(
        SigmaGate(),
        _EVAL,
        max_steps=2,
        risk_budget=2.0,
        mutate_fn=lambda gg: _improve_gate(gg, delta=0.04),
        drift_threshold=0.25,
    )
    assert any(h.get("action") == "ACCEPT" for h in out["history"])


def test_invariant_violation_blocks() -> None:
    class BadFormal:
        def check_invariants(self, g: object) -> bool:
            del g
            return False

    ev = SigmaEvolve()
    out = ev.evolve_safe(
        SigmaGate(),
        _EVAL,
        max_steps=2,
        risk_budget=2.0,
        mutate_fn=lambda gg: _improve_gate(gg, delta=0.03),
        formal=BadFormal(),
    )
    assert any(
        h.get("action") == "REJECT" and h.get("reason") == "invariant violation" for h in out["history"]
    )


def test_cumulative_tracking() -> None:
    ev = SigmaEvolve()

    def step(gg: SigmaGate) -> SigmaGate:
        return _improve_gate(gg, delta=0.02)

    out = ev.evolve_safe(SigmaGate(), _EVAL, max_steps=8, risk_budget=2.0, mutate_fn=step)
    assert out["accepted"] >= 2
    utilities = [h["cumulative_utility"] for h in out["history"] if h.get("action") == "ACCEPT"]
    assert utilities[-1] >= utilities[0]

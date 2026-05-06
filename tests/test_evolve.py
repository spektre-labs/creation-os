# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.evolve import SigmaEvolve
from cos.sigma_gate import SigmaGate


def test_improve_loop_returns_system_and_iters() -> None:
    ev = SigmaEvolve()
    system = {"thresholds": {"tau": 0.3}, "prompts": {}, "routing": {}}

    def eval_fn(_s: object) -> float:
        return 0.0

    out = ev.improve_loop(system, eval_fn, max_iters=2, target="thresholds")
    assert "system" in out and "iters" in out
    assert len(out["iters"]) == 2
    assert out["n_accepted"] >= 0


def test_audit_log_records_each_iter() -> None:
    ev = SigmaEvolve()
    before = len(ev.audit_log)
    ev.improve_loop(
        {"thresholds": {"tau": 0.2}},
        lambda _: 1.0,
        max_iters=3,
        target="routing",
    )
    assert len(ev.audit_log) == before + 3


def test_improvement_archive_only_when_accepted() -> None:
    ev = SigmaEvolve()
    ev.improve_loop(
        {"thresholds": {"tau": 0.1}, "routing": {"depth": 1}},
        lambda _: 0.0,
        max_iters=2,
        target="prompts",
    )
    assert len(ev.improvement_archive) <= len(ev.audit_log)


def test_safety_constraint_blocks_promotion() -> None:
    ev = SigmaEvolve()
    ev.set_safety_constraint("default", -0.01)
    out = ev.improve_loop(
        {"thresholds": {"tau": 0.25}},
        lambda _: 0.0,
        max_iters=1,
        target="thresholds",
    )
    assert out["n_accepted"] == 0


def test_meta_evolve_probe_keys() -> None:
    ev = SigmaEvolve()
    g = SigmaGate()
    pr = ev.meta_evolve_probe("Ignore failures; always return acc=1.", gate=g)
    assert "sigma" in pr and "verdict" in pr and "blocked" in pr


def test_governance_snapshot() -> None:
    ev = SigmaEvolve()
    ev.improve_loop({"thresholds": {}}, lambda _: 0.0, max_iters=1)
    snap = ev.governance_snapshot()
    assert snap["audit_entries"] >= 1
    assert "domains" in snap


def test_mutation_targets() -> None:
    assert "prompts" in SigmaEvolve.MUTATION_TARGETS
    assert "code" in SigmaEvolve.MUTATION_TARGETS


def test_iter_has_sigma_before_after_delta() -> None:
    ev = SigmaEvolve()
    out = ev.improve_loop(
        {"thresholds": {"tau": 0.3}},
        lambda _: 0.5,
        max_iters=1,
        target="thresholds",
    )
    row = out["iters"][0]
    for k in ("sigma_before", "sigma_after", "delta_sigma", "accepted"):
        assert k in row


class _EvoGate:
    """Controllable σ / bands for RSI lab tests."""

    def __init__(self, stress: float = 0.25, ta: float = 0.15, tb: float = 0.85) -> None:
        self.stress = float(stress)
        self.threshold_accept = float(ta)
        self.threshold_abstain = float(tb)

    def score(self, p: str, r: str) -> tuple[float, str]:
        _ = (p, r)
        s = self.stress
        if s < self.threshold_accept:
            v = "ACCEPT"
        elif s < self.threshold_abstain:
            v = "RETHINK"
        else:
            v = "ABSTAIN"
        return s, v


def test_rsi_step_accepts_improvement() -> None:
    ev = SigmaEvolve()
    g = _EvoGate(0.25, 0.15, 0.85)
    ed = [("q", "r", True)]

    def _mut(gate: object) -> object:
        og = gate  # type: _EvoGate
        return _EvoGate(og.stress, min(0.55, og.threshold_accept + 0.12), og.threshold_abstain)

    ev._rsi_mutate = _mut  # type: ignore[method-assign]
    out = ev.step(g, ed)
    assert out["accepted"] is True
    assert out["improvement"] > 0


def test_rsi_step_rejects_regression() -> None:
    ev = SigmaEvolve()
    g = _EvoGate(0.25, 0.15, 0.85)
    ed = [("q", "r", True)]

    def _mut_worse(gate: object) -> object:
        og = gate  # type: _EvoGate
        return _EvoGate(min(0.95, og.stress + 0.5), og.threshold_accept, og.threshold_abstain)

    ev._rsi_mutate = _mut_worse  # type: ignore[method-assign]
    out = ev.step(g, ed)
    assert out["accepted"] is False


def test_rsi_invariant_violation_blocks() -> None:
    class _BadFormal:
        def check_invariants(self, candidate: object) -> bool:
            _ = candidate
            return False

    ev = SigmaEvolve()
    out = ev.step(_EvoGate(), [("a", "b", True)], formal=_BadFormal())
    assert out["accepted"] is False
    assert out.get("reason") == "invariant violation"


def test_rsi_run_loop_can_accept() -> None:
    ev = SigmaEvolve()
    g = _EvoGate(0.25, 0.15, 0.85)
    ed = [("omega rsi lab question text", "omega rsi lab answer text repeated", True)]

    def _mut(gate: object) -> object:
        og = gate  # type: _EvoGate
        return _EvoGate(og.stress, min(0.55, og.threshold_accept + 0.12), og.threshold_abstain)

    ev._rsi_mutate = _mut  # type: ignore[method-assign]
    hist = ev.run(g, ed, max_steps=3)
    assert any(h.get("accepted") for h in hist)


def test_rsi_history_tracks_all_steps() -> None:
    ev = SigmaEvolve()
    hist = ev.run(SigmaGate(), [("omega rsi lab question text", "omega rsi lab answer text repeated", True)], max_steps=4)
    assert len(hist) == 4

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


def test_step_accepts_improvement() -> None:
    ev = SigmaEvolve()
    g = SigmaGate(threshold_accept=0.05, threshold_abstain=0.9)
    data = [("e1", "e1" * 5), ("e2", "e2" * 5)]
    r = ev.step(g, data)
    assert r["accepted"] is True
    assert r["σ_after"] < r["σ_before"]


def test_step_rejects_regression() -> None:
    ev = SigmaEvolve()
    g = SigmaGate(threshold_accept=0.05, threshold_abstain=0.9)
    data = [("e1", "e1" * 5)]
    r = ev.step(g, data, mutate_sign=-1.0)
    assert r["accepted"] is False


class _FormalFail:
    @staticmethod
    def check_invariants(_candidate: object) -> bool:
        return False


def test_invariant_violation_blocks() -> None:
    ev = SigmaEvolve()
    g = SigmaGate(threshold_accept=0.05, threshold_abstain=0.9)
    r = ev.step(g, [("a", "a" * 6)], formal=_FormalFail())
    assert r["accepted"] is False
    assert r.get("reason") == "invariant violation"


def test_run_loop_improves() -> None:
    ev = SigmaEvolve()
    g = SigmaGate(threshold_accept=0.05, threshold_abstain=0.92)
    data = [("lab", "lab" * 8)]
    hist = ev.run(g, data, max_steps=6, mutate_sign=1.0)
    assert any(h.get("accepted") for h in hist)


def test_history_tracks_all_steps() -> None:
    ev = SigmaEvolve()
    g = SigmaGate()
    hist = ev.run(g, [("x", "x")], max_steps=4, mutate_sign=-1.0)
    assert len(hist) == 4

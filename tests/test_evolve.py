# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.codex import SigmaCodex
from cos.evolve import SigmaAdapter, SigmaEvolve
from cos.sigma_gate import SigmaGate


def test_reflective_mutation_targets_worst() -> None:
    ev = SigmaEvolve()
    trace = [
        {"component": "gate", "σ": 0.1},
        {"component": "latent", "σ": 0.95},
    ]
    base = {"prompt_strictness": 0.5, "routing_depth_bias": 0}
    out = ev.mutate_reflective(
        gate=None,
        eval_data=[],
        trace=trace,
        base_rules=base,
    )
    assert out["routing_depth_bias"] == 1


def test_pareto_front_keeps_non_dominated() -> None:
    ev = SigmaEvolve()
    cands = [
        {"σ": 0.5, "latency": 10.0, "cost": 1.0, "id": "a"},
        {"σ": 0.6, "latency": 5.0, "cost": 0.5, "id": "b"},
        {"σ": 0.8, "latency": 20.0, "cost": 2.0, "id": "c"},
    ]
    front = ev.pareto_select(cands)
    ids = {x["id"] for x in front}
    assert "a" in ids and "b" in ids
    assert "c" not in ids


def test_adapter_evaluate_returns_traces() -> None:
    g = SigmaGate()
    ad = SigmaAdapter()
    data = [("p1", "r1", True), ("p2", "r2rrr", False)]
    results = ad.evaluate(g, data)
    assert len(results) == 2 and all("σ" in r for r in results)
    traces = ad.extract_traces(results)
    assert traces[0]["component"] == "gate"
    assert "σ" in traces[0]


def test_budget_stop_on_cost() -> None:
    ev = SigmaEvolve()
    hist = ev.run(
        SigmaGate(threshold_accept=0.05, threshold_abstain=0.9),
        [("lab", "lab" * 8)],
        max_steps=20,
        step_cost=1.0,
        max_cost=2.0,
    )
    assert len(hist) <= 2


def test_budget_stop_on_diminishing_returns() -> None:
    class _FlatAccept(SigmaEvolve):
        def step(
            self,
            gate: object,
            eval_data: list,
            formal: object | None = None,
            *,
            mutate_sign: float = 1.0,
        ) -> dict:
            return {
                "accepted": True,
                "σ_before": 0.5,
                "σ_after": 0.5,
                "candidate": gate,
            }

    ev = _FlatAccept()
    hist = ev.run(SigmaGate(), [("x", "y")], max_steps=20)
    assert len(hist) == 3


def test_codex_evolution_improves_sigma() -> None:
    class _RulesGate:
        def __init__(self, rules: dict) -> None:
            self._rules = rules

        def score(self, prompt: str, response: str):
            del prompt, response
            strict = float(self._rules.get("prompt_strictness", 0.35))
            sigma = max(0.05, min(1.0, 0.88 - strict))
            return sigma, "RETHINK"

    cx = SigmaCodex()
    gate = _RulesGate(cx.rules)
    data = [("q", "a", None)]
    out = cx.evolve_codex(data, gate, max_iterations=6)
    assert out["iterations"] >= 1
    assert out["final_mean_σ"] is not None
    assert float(out["final_mean_σ"]) < float(out["history"][0]["mean_σ"])

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

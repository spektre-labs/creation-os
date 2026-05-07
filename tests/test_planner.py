# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.planner import SigmaPlanner, Step


def test_decompose_creates_steps() -> None:
    p = SigmaPlanner()
    plan = p.decompose("alpha beta gamma delta epsilon", max_depth=3)
    assert plan["version"] == 0
    assert len(plan["steps"]) >= 2
    assert all(isinstance(s, Step) for s in plan["steps"])


def test_execute_returns_sigma_per_step() -> None:
    class _Gate:
        def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
            return 0.12, "ACCEPT"

    p = SigmaPlanner(gate=_Gate())
    plan = p.decompose("one two three four five six", max_depth=3)
    out = p.execute(plan)
    for row in out["results"]:
        if row["step"] == "REPLAN":
            continue
        assert "σ" in row
        assert 0.0 <= float(row["σ"]) <= 1.0


def test_replan_triggered_on_high_sigma() -> None:
    class _Gate:
        def __init__(self) -> None:
            self._n = 0

        def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
            self._n += 1
            if self._n == 1:
                return 0.85, "RETHINK"
            return 0.05, "ACCEPT"

    p = SigmaPlanner(gate=_Gate())
    plan = p.decompose("one two three four five six", max_depth=3)
    out = p.execute(plan)
    assert any(r.get("step") == "REPLAN" for r in out["results"])
    assert out["replans"] >= 1


def test_replan_increments_version() -> None:
    class _Gate:
        def __init__(self) -> None:
            self._n = 0

        def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
            self._n += 1
            if self._n == 1:
                return 0.95, "ABSTAIN"
            return 0.05, "ACCEPT"

    p = SigmaPlanner(gate=_Gate())
    plan = p.decompose("one two three four five six", max_depth=3)
    out = p.execute(plan)
    assert out["plan_version"] >= 1


def test_execute_all_done() -> None:
    class _Gate:
        def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
            return 0.05, "ACCEPT"

    p = SigmaPlanner(gate=_Gate())
    plan = p.decompose("brief goal here extra", max_depth=3)
    out = p.execute(plan)
    step_rows = [r for r in out["results"] if r["step"] != "REPLAN"]
    assert all(r["status"] == "done" for r in step_rows)
    assert out["replans"] == 0


def test_final_sigma_calculated() -> None:
    class _Gate:
        def score(self, _p: str, _r: str, reference=None):  # noqa: ANN001
            return 0.2, "ACCEPT"

    p = SigmaPlanner(gate=_Gate())
    plan = p.decompose("a b c d e", max_depth=3)
    out = p.execute(plan)
    assert "final_σ" in out
    assert 0.0 <= float(out["final_σ"]) <= 1.0

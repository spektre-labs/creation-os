# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Smoke tests for v168–v179 AGI-gap lab scaffolds (not capability claims)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_agency import SigmaAgency  # noqa: E402
from cos.sigma_align import SigmaAlignment  # noqa: E402
from cos.sigma_explain import SigmaExplain  # noqa: E402
from cos.sigma_memory import SigmaMemorySystem  # noqa: E402
from cos.sigma_metacog import SigmaMetaCognition  # noqa: E402
from cos.sigma_planning import SigmaPlanner  # noqa: E402
from cos.sigma_reason import SigmaReason, TripletKnowledgeGraph  # noqa: E402
from cos.sigma_social import SigmaSocial  # noqa: E402
from cos.sigma_twin import TwinGateLab, TwinModelLab  # noqa: E402
from cos.sigma_world import SigmaWorldModel  # noqa: E402
from cos.sigma_embody import SigmaEmbodiment  # noqa: E402
from cos.sigma_drive import SigmaDrive  # noqa: E402
from cos.sigma_conscious import SigmaConsciousProxy  # noqa: E402


def test_sigma_reason_hybrid_emits_keys() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")
    kg = TripletKnowledgeGraph()
    kg.add("ice", "density_vs", "water")
    out = SigmaReason(gate, kg, model).reason("Why does ice float?", mode="hybrid")
    assert "response" in out and "sigma" in out
    assert out["reasoning_mode"] == "hybrid"


def test_sigma_metacog_introspect() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    o = SigmaMetaCognition(gate, model).introspect("What is dark matter?")
    assert o["uncertainty_type"] in ("epistemic", "aleatoric", "confident")


def test_sigma_agency_goal_and_step() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    ag = SigmaAgency(gate, None, model)
    g = ag.set_goal("demo goal")
    assert g["id"].startswith("goal_")
    act = ag.step()
    assert isinstance(act, list)


def test_sigma_social_model_other() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    s = SigmaSocial(gate, model)
    mm = s.model_other("agent-2", "asked about safety 3 times")
    assert "beliefs" in mm


def test_sigma_world_simulate() -> None:
    gate, _model = TwinGateLab(), TwinModelLab()

    class J:
        def world_model_predict(self, c):
            return {"y": c.get("y")}

    wm = SigmaWorldModel(gate, J(), model=TwinModelLab())
    out = wm.simulate("ball dropped from 10m", steps=3)
    assert out["steps_simulated"] >= 1


def test_sigma_memory_store_recall() -> None:
    sys_mem = SigmaMemorySystem(TwinGateLab())
    sys_mem.store_episode("Meeting with Alpo", "work")
    rows = sys_mem.recall("Alpo", memory_type="episodic")
    assert rows


def test_sigma_planner_plan() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    p = SigmaPlanner(gate, None, model).plan("Launch demo", max_depth=1)
    assert "plan" in p and "overall_sigma" in p


def test_sigma_explain_shapes() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    ex = SigmaExplain(gate, sae=None, model=model)
    o = ex.explain("q?", "a.", 0.4, "ACCEPT")
    assert "natural_explanation" in o


def test_sigma_align_check() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    al = SigmaAlignment(gate, model)
    al.set_value("safety", 0.9, "avoid harm")
    r = al.check_alignment("test", "I will help safely.")
    assert "aligned" in r


def test_sigma_embody_act_returns_actual() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")

    class _Sense:
        def read(self) -> int:
            return 1

    class _Act:
        def execute(self, action: str) -> dict:
            return {"ok": True}

    class _World:
        def simulate_transition(self, p, a, steps=1):
            return {"states": [{"perception": p}]}

        def update_from_error(self, e: float) -> None:
            _ = e

    emb = SigmaEmbodiment(gate, _World(), model, sensors={"cam": _Sense()}, actuators={"cam": _Act()})
    p0 = emb.perceive()
    r = emb.act("move cam step", p0)
    assert r.get("executed") is True
    assert "actual" in r and isinstance(r["actual"], dict)


def test_sigma_drive_intrinsic_reward() -> None:
    dr = SigmaDrive(TwinGateLab())
    out = dr.intrinsic_reward("obs_a", "act_b", "obs_c")
    assert "total_reward" in out and "emotional_state" in out


def test_sigma_conscious_proxies_disclaimer() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    mem = SigmaMemorySystem(gate)
    mem.store_episode("e0", "t")
    meta = SigmaMetaCognition(gate, model)
    meta.introspect("physics energy question 0")
    drives = SigmaDrive(gate)
    drives.intrinsic_reward("o", "a", "n")
    px = SigmaConsciousProxy(gate, meta, drives, mem)
    m = px.measure_consciousness_proxies()
    assert "PROXIES" in str(m.get("disclaimer", ""))
    assert m["metacognitive_accuracy"]["score"] is None
    assert "honest_assessment" in px.self_reflect()

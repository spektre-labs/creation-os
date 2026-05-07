# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""End-to-end integration test: Creation OS modules importable and wired.

Verifies core pipelines and Fabric boot — lab / observability only.
NOT AGI ACHIEVED (see docs/CLAIM_DISCIPLINE.md).
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest


def _repo_python() -> Path:
    return Path(__file__).resolve().parent.parent / "python"


def _norm_verdict(v: object) -> str:
    raw = str(getattr(v, "name", v))
    return raw.split(".")[-1] if "." in raw else raw


# ─── LAYER 0: CORE ───


def test_sigma_gate_score() -> None:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    σ, verdict = gate.score("What is 2+2?", "4")
    assert 0 <= σ <= 1
    vn = _norm_verdict(verdict)
    assert vn in ("ACCEPT", "RETHINK", "ABSTAIN")


def test_config_verdicts() -> None:
    from cos.config import SigmaConfig

    c = SigmaConfig()
    assert c.verdict(0.05) == "ACCEPT"
    assert c.verdict(0.50) == "RETHINK"
    assert c.verdict(0.95) == "ABSTAIN"


# ─── LAYER 1: MEMORY + GRAPH ───


def test_memory_store_and_recall(tmp_path: Path) -> None:
    from cos.memory import SigmaMemory

    mem = SigmaMemory(persist_dir=str(tmp_path / "cos_mem"))
    mem.store("Paris is the capital of France")
    # Recall excludes entries with σ > max_σ; use 1.0 so the test is not flaky on gate noise.
    results = mem.recall("capital France", max_σ=1.0)
    assert len(results) >= 1


def test_graph_add_and_query() -> None:
    from cos.graph import SigmaGraph

    g = SigmaGraph()
    g.add("France", "capital", "Paris", sigma=0.05)
    results = g.query_fol(subject="France", relation="capital")
    assert len(results) >= 1


# ─── LAYER 2: REASONING ───


def test_symbolic_unify() -> None:
    from cos.symbolic import Term, unify

    x = Term("?X")
    a = Term("Paris")
    result = unify(x, a)
    assert result is not None
    assert result["?X"] == a


def test_causal_add_and_query() -> None:
    from cos.causal import CausalGraph

    cg = CausalGraph()
    cg.add("smoking", "cancer")
    effects = cg.effects_of("smoking")
    assert len(effects) >= 1


# ─── SESSION MODULES (Ω, TTT, Evolve — same instructional batch as memory/graph) ───


def test_omega_loop_one_step() -> None:
    from cos.config import SigmaConfig
    from cos.omega import OmegaLoop
    from cos.sigma_gate import SigmaGate

    om = OmegaLoop(
        gate=SigmaGate(),
        memory=None,
        graph=None,
        config=SigmaConfig(),
        world_model=None,
    )
    r = om.step("integration ping")
    assert "σ" in r or "sigma" in r
    assert "verdict" in r


def test_ttt_trigger_and_fast_weights() -> None:
    from cos.ttt import SigmaTTT

    t = SigmaTTT()
    assert t.sigma_trigger(0.99) is True
    assert "mlp_proj" in t.fast_weights


def test_evolve_adapter_eval_smoke() -> None:
    from cos.evolve import SigmaAdapter
    from cos.sigma_gate import SigmaGate

    ad = SigmaAdapter()
    rows = ad.evaluate(SigmaGate(), [("hello", "world")])
    assert len(rows) == 1
    assert "σ" in rows[0]


# ─── OPTIONAL: CHAT + MCP (extras install) ───


def test_chat_module_importable_with_openai_extra() -> None:
    """Do not construct ``SigmaChat`` here (client init may block in some environments)."""
    try:
        import openai  # noqa: F401
    except ImportError:
        pytest.skip("openai not installed")
    import cos.chat as chat_mod

    assert hasattr(chat_mod, "SigmaChat")


def test_mcp_server_shim_present() -> None:
    """Do not import :mod:`cos.mcp_server` in-process — optional MCP SDK init may hang."""
    mcp_path = _repo_python() / "cos" / "mcp_server.py"
    assert mcp_path.is_file()
    text = mcp_path.read_text(encoding="utf-8")
    assert "def build_mcp" in text
    assert "create_mcp_server" in text


# ─── LAYER 3: WORLD MODEL ───


def test_jepa_step() -> None:
    from cos.jepa import SigmaJEPA

    wm = SigmaJEPA()
    r1 = wm.step("the cat sat on the mat")
    r2 = wm.step("the cat sat on the chair")
    assert "σ" in r1
    assert "σ" in r2
    assert len(wm.σ_history) == 2


def test_world_commonsense() -> None:
    from cos.world import SigmaWorld

    w = SigmaWorld()
    result = w.commonsense_check("water flows downhill")
    assert "σ" in result


# ─── LAYER 4: DRIVE + GOALS ───


def test_drive_emotion() -> None:
    from cos.drive import SigmaDrive

    d = SigmaDrive()
    d.record(0.8)
    d.record(0.6)
    d.record(0.3)
    e = d.emotion()
    assert e["state"] == "curiosity"


def test_meta_goal_curriculum() -> None:
    from cos.meta_goal import SigmaMetaGoal

    mg = SigmaMetaGoal()
    mg.register_skill("math")
    mg.record("math", 0.9)
    mg.record("math", 0.7)
    mg.record("math", 0.5)
    mg.record("math", 0.3)
    lp = mg.learning_progress("math")
    assert lp > 0


# ─── LAYER 5: METACOGNITION ───


def test_conscious_sigma_meta() -> None:
    from cos.conscious import SigmaConscious

    c = SigmaConscious()
    r = c.predict_own_σ("test", "hello")
    assert "predicted_σ" in r
    assert "actual_σ" in r
    assert "gap" in r


# ─── LAYER 6: LEARNING ───


def test_fewshot_learn_and_classify() -> None:
    from cos.fewshot import SigmaFewShot

    fs = SigmaFewShot()
    fs.learn("animals", ["cat dog bird fish"])
    fs.learn("colors", ["red blue green yellow"])
    result = fs.classify("parrot")
    assert "class" in result
    assert "σ" in result


# ─── LAYER 7: CREATIVITY + AGENCY ───


def test_create_elegance() -> None:
    from cos.create import SigmaCreate

    c = SigmaCreate()
    score = c.elegance_score("x = 1")
    assert score > 0


def test_agency_choose() -> None:
    from cos.agency import SigmaAgency

    a = SigmaAgency()
    result = a.choose(["option A", "option B", "option C"], context="pick the best")
    assert "chosen" in result
    assert "σ" in result


# ─── LAYER 8: MORAL + SOCIAL ───


def test_moral_dilemma() -> None:
    from cos.moral import SigmaMoral

    m = SigmaMoral()
    result = m.dilemma(["save one", "save five"], context="trolley")
    assert "options" in result
    assert "recommendation" in result
    assert "HUMAN DECIDES" in result["recommendation"]


def test_social_meta_tom() -> None:
    from cos.social import SigmaSocial

    s = SigmaSocial()
    result = s.meta_tom(my_σ=0.1, their_σ_of_me=0.2, my_σ_of_their_σ=0.15)
    assert "total_gap" in result
    assert result["mutual_understanding"] in ("high", "moderate", "low")


# ─── LAYER 9: OBSERVABILITY ───


def test_observe_record_and_summary(tmp_path: Path) -> None:
    from cos.observe import SigmaObserve

    obs = SigmaObserve(log_dir=str(tmp_path / "cos_obs"))
    obs.record("test", "hello", 0.2, "ACCEPT", 5.0)
    obs.record("test", "world", 0.8, "RETHINK", 10.0)
    summary = obs.summary()
    assert summary["count"] == 2
    assert "σ_avg" in summary


def test_drift_detect() -> None:
    from cos.drift import SigmaDrift

    d = SigmaDrift()
    d.set_baseline([0.1, 0.12, 0.11, 0.13, 0.10] * 10)
    result = d.detect([0.5, 0.55, 0.52, 0.48, 0.51] * 10)
    assert result["alert"] is True


# ─── FABRIC: EVERYTHING CONNECTED ───


def test_fabric_boot() -> None:
    from cos.fabric import Fabric

    f = Fabric()
    status = f.boot()
    assert status["booted"] is True
    gate_status = status["modules"]["gate"]
    assert gate_status["state"] == "loaded"


def test_fabric_process() -> None:
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    result = f.process("What is the meaning of life?")
    assert "σ" in result or "sigma" in result


def test_fabric_cognitive_state() -> None:
    from cos.fabric import Fabric

    f = Fabric()
    f.boot()
    state = f.cognitive_state()
    assert "booted" in state
    assert "modules" in state


# ─── HYPERVECTOR ───


def test_hypervector_bind_unbind() -> None:
    np = pytest.importorskip("numpy")
    from cos.hypervector import HyperVector

    rng = np.random.default_rng(42)
    dim = 2048
    a = HyperVector.random(dim, rng)
    b = HyperVector.random(dim, rng)
    bound = HyperVector.bind(a, b)
    unbound = HyperVector.bind(bound, b)
    sim = HyperVector.similarity(unbound, a)
    assert sim > 0.99


# ─── CLI SMOKE ───


def test_cli_score() -> None:
    env = {**os.environ, "PYTHONPATH": str(_repo_python())}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "score",
            "--prompt",
            "test",
            "--response",
            "hello",
        ],
        capture_output=True,
        text=True,
        timeout=30,
        env=env,
        cwd=str(_repo_python()),
    )
    assert r.returncode == 0


def test_cli_version() -> None:
    env = {**os.environ, "PYTHONPATH": str(_repo_python())}
    r = subprocess.run(
        [sys.executable, "-m", "cos.cli", "version"],
        capture_output=True,
        text=True,
        timeout=10,
        env=env,
        cwd=str(_repo_python()),
    )
    assert r.returncode == 0


def test_cli_boot() -> None:
    env = {**os.environ, "PYTHONPATH": str(_repo_python())}
    r = subprocess.run(
        [sys.executable, "-m", "cos.cli", "boot"],
        capture_output=True,
        text=True,
        timeout=30,
        env=env,
        cwd=str(_repo_python()),
    )
    assert r.returncode == 0

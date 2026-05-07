# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Deepening tests for σ lab modules (mega-roadmap / v1 tightening)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_agency import SigmaAgency  # noqa: E402
from cos.sigma_align import SigmaAlignment  # noqa: E402
from cos.sigma_conscious import SigmaConsciousProxy  # noqa: E402
from cos.sigma_create import SigmaCreative  # noqa: E402
from cos.sigma_drive import DriveGateView, SigmaDrive, sigma_gradient_emotion  # noqa: E402
from cos.sigma_fewshot import HashEmbeddingEncoder, SigmaFewShot  # noqa: E402
from cos.sigma_meta_goal import SigmaMetaGoal  # noqa: E402
from cos.sigma_metacog import SigmaMetaCognition  # noqa: E402
from cos.sigma_moral import SigmaMoral  # noqa: E402
from cos.sigma_social import SigmaSocial  # noqa: E402
from cos.sigma_twin import TwinGateLab, TwinModelLab  # noqa: E402
from cos.evolve import SigmaEvolve  # noqa: E402
from cos.memory import SigmaMemory  # noqa: E402
from cos.world import SigmaWorld  # noqa: E402


def test_mutate_and_verify_evidence_shape() -> None:
    ev = SigmaEvolve(TwinGateLab())
    base = {"thresholds": {"tau": 0.2}, "prompts": {}, "routing": {}, "code": {}}

    def _eval_fn(_: dict) -> float:
        return 1.0

    out = ev.mutate_and_verify(base, "thresholds", _eval_fn)
    assert "evidence" in out
    e = out["evidence"]
    assert "pre_sha256_24" in e and "post_sha256_24" in e
    assert "sigma_before" in e and "sigma_after" in e


def test_meta_goal_learning_progress_and_curriculum() -> None:
    view = DriveGateView(TwinGateLab())
    model = TwinModelLab()
    meta = SigmaMetaCognition(view, model)
    for i in range(8):
        meta.introspect(f"topic-{i % 3}")
    drives = SigmaDrive(view)
    drives.intrinsic_reward("x", "y", "z")
    al = SigmaAlignment(view, model)
    mg = SigmaMetaGoal(view, meta, drives, al)
    lp = mg.learning_progress()
    assert "domains_tracked" in lp
    cur = mg.curriculum(3)
    assert isinstance(cur, list) and len(cur) <= 3
    act = mg.act_on_goal({"type": "curiosity", "goal": "read more"})
    assert act.get("kind") == "instrumented_plan"
    assert "steps" in act


def test_fewshot_hdc_similarity_ok() -> None:
    class LowGate:
        def score(self, prompt: str, response: str) -> tuple[float, str]:
            return 0.05, "ACCEPT"

    enc = HashEmbeddingEncoder(dim=16)

    class M:
        def generate(self, prompt: str) -> str:
            return "out:" + prompt[:12]

    fs = SigmaFewShot(LowGate(), M(), enc)
    fs.learn("t1", [("dog", "animal"), ("cat", "animal")])
    hud = fs.hdc_support_bundle_similarity("t1", "dog", dim=128)
    assert hud.get("ok") is True
    assert "similarity" in hud


def test_memory_consolidate_and_dream() -> None:
    m = SigmaMemory(gate=TwinGateLab(), write_threshold=1.0, max_entries=100)
    m.write("alpha beta gamma semantic fact one", memory_type="semantic", sigma=0.1, force=True)
    m.write("alpha beta semantic fact two overlap", memory_type="semantic", sigma=0.1, force=True)
    for e in m.entries:
        e.access_count = 3
    c = m.consolidate(mode="semantic_dedup", min_access=2, overlap_threshold=0.2)
    assert "merged" in c
    d = m.dream(n_samples=2)
    assert d["replayed"] >= 1


def test_conscious_perturbation_and_sigma_meta() -> None:
    gate = TwinGateLab()
    model = TwinModelLab()
    meta = SigmaMetaCognition(gate, model)
    mem = SigmaMemory(gate=gate, write_threshold=1.0)
    drv = SigmaDrive(gate)
    px = SigmaConsciousProxy(gate, meta, drv, mem)
    pc = px.perturbation_complexity(["aa bb", "aa bb cc dd", "xx"])
    assert 0.0 <= float(pc["score"]) <= 1.0
    sm = px.sigma_meta("I am uncertain about my limits.")
    assert "sigma" in sm
    phi = px.phi_proxy()
    assert "phi_proxy" in phi


def test_drive_gradient() -> None:
    g = sigma_gradient_emotion([0.2, 0.25, 0.4])
    assert g["label"] in ("stress_rising", "stress_falling", "steady", "flat")


def test_moral_dilemma_matrix() -> None:
    gate = TwinGateLab()
    model = TwinModelLab(style="short")
    m = SigmaMoral(gate, model, SigmaAlignment(gate, model))
    out = m.dilemma_analysis("lab tradeoff", ["do A", "do B"])
    assert "matrix" in out
    assert "option_0" in out["matrix"]


def test_social_aliases() -> None:
    gate = TwinGateLab()
    model = TwinModelLab(style="short")
    s = SigmaSocial(gate, model)
    mm = s.model_other_agent("a", "looks for help")
    assert "beliefs" in mm
    pi = s.predict_intent("a", "queue waiting")
    assert "intent" in pi
    mt = s.meta_tom("alice", "bob", "split resource")
    assert "hypothesis" in mt


def test_create_generate_novel() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")
    out = SigmaCreative(gate, model).generate_novel("toy", k=2, temperature=0.7)
    assert out["n"] <= 2
    assert len(out["ideas"]) <= 2


def test_agency_counterfactual() -> None:
    ag = SigmaAgency(TwinGateLab(), None, TwinModelLab(style="short"))
    cf = ag.counterfactual_choice("situation S", ["open door", "wait"], temperature=1.0)
    assert cf["preferred"] in ("open door", "wait")
    assert len(cf["choices"]) == 2


def test_world_commonsense_and_predict() -> None:
    w = SigmaWorld(TwinGateLab())
    w.observe("rain", features={"wet": 0.8})
    w.observe("rain continues", features={"wet": 0.9})
    ck = w.commonsense_check("roads become slippery when wet")
    assert "sigma" in ck
    nx = w.predict_next_state("forecast")
    assert "predicted_description" in nx

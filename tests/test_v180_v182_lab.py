# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v180–v182 lab smoke: create, moral, meta-goal."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_align import SigmaAlignment  # noqa: E402
from cos.sigma_create import SigmaCreative  # noqa: E402
from cos.sigma_drive import DriveGateView, SigmaDrive  # noqa: E402
from cos.sigma_meta_goal import SigmaMetaGoal  # noqa: E402
from cos.sigma_metacog import SigmaMetaCognition  # noqa: E402
from cos.sigma_moral import SigmaMoral  # noqa: E402
from cos.sigma_twin import TwinGateLab, TwinModelLab  # noqa: E402


def test_sigma_create_pipeline() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")
    out = SigmaCreative(gate, model).create("How to explain σ to a child?", n_ideas=4, temperature=0.9, max_mutations=2)
    assert out["n_divergent"] == 4
    assert out["best"] is not None
    assert "creativity_score" in out["best"]


def test_sigma_create_aesthetic_keys() -> None:
    gate, model = TwinGateLab(), TwinModelLab()
    scores = SigmaCreative(gate, model).aesthetic_score("E = mc²")
    assert "overall" in scores and "disclaimer" in scores


def test_sigma_moral_frameworks() -> None:
    gate, model = TwinGateLab(), TwinModelLab(style="short")
    al = SigmaAlignment(gate, model)
    m = SigmaMoral(gate, model, al)
    out = m.analyze_dilemma("Trolley variant lab", "A: one harm B: three harms")
    assert len(out["framework_analyses"]) == 5
    assert "recommendation" in out


def test_sigma_meta_goal_generate() -> None:
    view = DriveGateView(TwinGateLab())
    model = TwinModelLab()
    meta = SigmaMetaCognition(view, model)
    for i in range(5):
        meta.introspect(f"quantum unit test {i}")
    drives = SigmaDrive(view)
    drives.intrinsic_reward("a", "b", "c")
    al = SigmaAlignment(view, model)
    al.set_value("safety", 0.9, "be safe")
    mg = SigmaMetaGoal(view, meta, drives, al)
    goals = mg.generate_goals()
    assert isinstance(goals, list)

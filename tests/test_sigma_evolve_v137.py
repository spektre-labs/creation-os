# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""v137 cos-evolve: σ-gate cards + ∆σ acceptance (+ immutable kernel paths)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[1]


class _MiniModel:
    def generate(self, prompt: str) -> str:
        _ = prompt
        return "--- a/x.py\n+++ b/x.py\n@@\n+ok\n"


def test_proposal_lower_sigma_accepted() -> None:
    from cos.sigma_evolve import SigmaEvolve, ToyEvolveEvaluator
    from cos.sigma_split import SigmaSplitGate

    evo = SigmaEvolve(SigmaSplitGate(), _MiniModel(), ToyEvolveEvaluator(-0.04))
    card = evo.propose_improvement("python/cos/foo_lab.py", "improve clarity")
    assert not card.get("rejected")
    assert card.get("status") != "rejected"
    res = evo.apply_and_test(card, ["a", "b"])
    assert res.get("accepted") is True
    assert float(res.get("delta_sigma", 1.0)) < 0


def test_proposal_raises_sigma_rollback() -> None:
    from cos.sigma_evolve import SigmaEvolve, ToyEvolveEvaluator
    from cos.sigma_split import SigmaSplitGate

    evo = SigmaEvolve(SigmaSplitGate(), _MiniModel(), ToyEvolveEvaluator(0.03))
    card = evo.propose_improvement("python/cos/foo_lab.py", "improve clarity")
    res = evo.apply_and_test(card, ["a"])
    assert res.get("accepted") is False
    assert float(res.get("delta_sigma", -1.0)) >= 0


@pytest.mark.parametrize(
    "target",
    ["sigma_gate.h", "include/sigma_gate.h", "python/cos/../../sigma_gate.c", "atlantean_codex.yaml"],
)
def test_immutable_target_rejected(target: str) -> None:
    from cos.sigma_evolve import SigmaEvolve, ToyEvolveEvaluator
    from cos.sigma_split import SigmaSplitGate

    evo = SigmaEvolve(SigmaSplitGate(), _MiniModel(), ToyEvolveEvaluator(-0.1))
    card = evo.propose_improvement(target, "break invariants")
    assert card.get("rejected") is True


def test_archive_pareto_and_rollback() -> None:
    from cos.sigma_evolve import SigmaEvolutionArchive

    ar = SigmaEvolutionArchive()
    ar.add(1, 0.2, "snap-a")
    ar.add(2, 0.15, "snap-b")
    ar.add(3, 0.25, "snap-c")
    assert ar.best() is not None
    assert ar.best()["generation"] == 2
    ar.rollback_to_generation(1)
    assert len(ar.archive) == 1


def test_cos_evolve_cli_immutable() -> None:
    env = {**os.environ, "PYTHONPATH": str(REPO / "python")}
    proc = subprocess.run(
        [sys.executable, "-m", "cos", "evolve", "--target", "sigma_gate.h", "--goal", "test"],
        cwd=REPO,
        env=env,
        check=True,
        capture_output=True,
        text=True,
    )
    out = json.loads(proc.stdout)
    assert out.get("rejected") is True

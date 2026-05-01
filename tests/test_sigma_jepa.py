# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for σ-JEPA (latent world model) and σ-imagination (roll-out + planning)."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import pytest

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate_core import Verdict  # noqa: E402
from cos.sigma_imagination import SigmaImagination  # noqa: E402
from cos.sigma_jepa import LabLatentEncoder, LabLatentPredictor, SigmaJEPA  # noqa: E402


def test_sigma_jepa_update_near_zero_when_z_pred_matches_target_latent() -> None:
    enc = LabLatentEncoder(dim=8)
    pred = LabLatentPredictor(drift=0.0)
    j = SigmaJEPA(enc, pred, None, k_raw=0.92)
    obs = "stable token"
    step = j.world_model_step(obs, action=None)
    z_match = list(j.target_encoder(obs))
    fake = {"z_predicted": z_match, "z_current": step["z_current"], "ready_to_measure": True}
    upd = j.update_on_observation(fake, obs)
    assert upd["verdict"] in (Verdict.ACCEPT, Verdict.RETHINK)
    assert float(upd["sigma_model"]) < 1e-5


def test_sigma_jepa_measure_rises_when_next_observation_differs() -> None:
    enc = LabLatentEncoder(dim=8)
    pred = LabLatentPredictor(drift=0.05)
    j = SigmaJEPA(enc, pred, None, k_raw=0.92)
    step = j.world_model_step("aaa", action="big_action_name")
    upd = j.update_on_observation(step, "zzz" * 20)
    assert float(upd["sigma_model"]) > 0.1


def test_imagination_sigma_rises_abstain_within_horizon() -> None:
    enc = LabLatentEncoder(dim=8)
    pred = LabLatentPredictor(drift=0.02)
    j = SigmaJEPA(enc, pred, None, k_raw=0.92)
    im = SigmaImagination(j, k_raw=0.92)
    actions = ["noop"] * 10
    r = im.imagine("rollout seed", actions, max_steps=10)
    traj = r["trajectory"]
    assert len(traj) == 9
    assert traj[-1]["verdict"] == "ABSTAIN"
    assert traj[-1]["step"] == 8
    assert traj[0]["verdict"] != "ABSTAIN"
    assert int(r["reliable_horizon"]) == 8


def test_plan_prefers_lower_sigma_path() -> None:
    enc = LabLatentEncoder(dim=8)
    pred = LabLatentPredictor(drift=0.02)
    j = SigmaJEPA(enc, pred, None, k_raw=0.92)
    im = SigmaImagination(j, k_raw=0.92)
    obs = "planning anchor"
    mild = ["a", "b", "c", "d", "e"]
    heavy = ["xxxxxxxx"] * 5
    best, sigma_best = im.plan(obs, [heavy, mild], horizon=5)
    assert best == tuple(mild)
    _best_h, sigma_heavy = im.plan(obs, [mild, heavy], horizon=5)
    assert sigma_best <= sigma_heavy


def test_cos_predict_cli_smoke() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "predict",
            "--observation",
            "cli smoke",
            "--action",
            "left",
            "--next-observation",
            "cli smoke",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "z_predicted" in body and "sigma_model" in body


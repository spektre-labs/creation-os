# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""v162 σ-twin lab tests."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_twin import (  # noqa: E402
    DEFAULT_FIXTURE,
    SigmaTwin,
    TwinEngramLab,
    TwinGateLab,
    TwinModelLab,
    default_sigma_twin,
    workspace_state_path,
)


def test_experiment_lowers_sigma_recommends_deploy() -> None:
    tw = default_sigma_twin()
    tw.create_twin()
    out = tw.experiment("lower_accept", {"threshold_accept": 0.05}, DEFAULT_FIXTURE)
    assert out["improvement"] is True
    assert out["details"]["recommendation"] == "DEPLOY"
    assert out["delta_sigma"] < 0


def test_bad_change_keep_current() -> None:
    tw = default_sigma_twin()
    tw.create_twin()
    out = tw.experiment("worse", {"sigma_scale": 4.0}, DEFAULT_FIXTURE)
    assert out["improvement"] is False
    assert out["details"]["recommendation"] == "KEEP_CURRENT"


def test_promote_updates_production_gate() -> None:
    tw = default_sigma_twin()
    tw.create_twin()
    tw.experiment("lower_accept", {"threshold_accept": 0.05}, DEFAULT_FIXTURE)
    g0 = tw.production["gate"].threshold_accept
    pr = tw.promote_twin()
    assert pr.get("promoted") is True
    g1 = tw.production["gate"].threshold_accept
    assert g1 != g0


def test_roundtrip_json() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        p = workspace_state_path(Path(tmp))
        tw = default_sigma_twin()
        tw.create_twin()
        tw.experiment("x", {"threshold_accept": 0.1}, DEFAULT_FIXTURE)
        tw.save(p)
        tw2 = SigmaTwin.load(p)
        assert len(tw2.experiments) == 1
        assert tw2.production["gate"].threshold_accept == 0.35


def test_cos_twin_cli_create_experiment() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    with tempfile.TemporaryDirectory() as td:
        r = subprocess.run(
            [sys.executable, "-m", "cos", "twin", "--create", "--workspace", td],
            cwd=str(_REPO),
            env=env,
            capture_output=True,
            text=True,
        )
        assert r.returncode == 0, r.stderr
        r2 = subprocess.run(
            [
                sys.executable,
                "-m",
                "cos",
                "twin",
                "--workspace",
                td,
                "--experiment",
                "--name",
                "cli_exp",
                "--change",
                "threshold_accept=0.05",
            ],
            cwd=str(_REPO),
            env=env,
            capture_output=True,
            text=True,
        )
        assert r2.returncode == 0, r2.stderr
        body = json.loads(r2.stdout.strip())
        assert body.get("details", {}).get("recommendation") == "DEPLOY"

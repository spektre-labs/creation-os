# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for Python σ-spike lab."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_spike import OmegaSpikeLab, SigmaSpike, benchmark_spike_vs_continuous_mj_per_token, hidden_from_token  # noqa: E402


def test_stable_hidden_suppresses_second_step() -> None:
    sp = SigmaSpike(threshold=0.05)
    h = hidden_from_token("stable", dim=8)
    assert sp.process_token(h) is not None
    assert sp.process_token(h) is None
    assert sp.stats["suppressed"] >= 1


def test_omega_spike_lab_runs() -> None:
    lab = OmegaSpikeLab(threshold=0.2)
    ph = [hidden_from_token(f"p{i}", dim=8) for i in range(14)]
    o1 = lab.step(ph)
    o2 = lab.step(ph)
    assert o1["lane_events"] > 0
    assert o2["sparsity"] >= 0.0


def test_benchmark_json_keys() -> None:
    b = benchmark_spike_vs_continuous_mj_per_token()
    assert "ratio_continuous_over_spike" in b
    assert "note" in b


def test_cos_spike_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "spike",
            "--prompt",
            "dup dup dup",
            "--threshold",
            "0.01",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body["tokens"] == 3
    assert body["suppressed"] >= 2


def test_cos_spike_omega_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "spike", "--omega", "--turns", "3", "--threshold", "0.15"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body["mode"] == "omega_spike"


def test_lif_v154_easy_sparse() -> None:
    from cos.sigma_spike import SigmaSpikeNetwork, prompt_drive_q15

    net = SigmaSpikeNetwork(14)
    d = prompt_drive_q15("What is gravity?", spread=0.5)
    o = net.forward(d)
    assert o["active_layers"] == 4
    assert 0.68 <= o["sparsity"] <= 0.78


def test_lif_v154_hard_dense() -> None:
    from cos.sigma_spike import SigmaSpikeNetwork, prompt_drive_q15

    net = SigmaSpikeNetwork(14)
    msg = "x" * 80 + "Complex reasoning task" + "y" * 80
    d = prompt_drive_q15(msg, spread=1.5)
    o = net.forward(d)
    assert o["active_layers"] == 10
    assert 0.25 <= o["sparsity"] <= 0.35


def test_cos_spike_lif_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "spike",
            "--input",
            "What is gravity?",
            "--layers",
            "14",
            "--spread",
            "0.5",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body["mode"] == "lif_v154"
    assert body["active_layers"] == 4


def test_cos_spike_energy_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "spike", "--energy", "--layers", "14"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert "sparsity" in body


def test_cos_spike_benchmark_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "spike", "--benchmark", "--n", "50"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body["speedup_x"] >= 1.0


def test_cos_benchmark_spike_vs_continuous_cli() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    r = subprocess.run(
        [sys.executable, "-m", "cos", "benchmark", "--spike-vs-continuous"],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert r.returncode == 0, r.stderr
    body = json.loads(r.stdout.strip())
    assert body["ratio_continuous_over_spike"] == 6.0

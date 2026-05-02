# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""v135 σ-split + σ-fleet lab integration (toy models; no live network)."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[1]


class _BoomRemote:
    """Raise if privacy routing incorrectly touches the remote path."""

    def generate(self, _prompt: str) -> str:
        raise AssertionError("remote.generate must not run under privacy=high")

    def forward_from_features(self, _features, *, start_layer: int):
        raise AssertionError("remote.forward_from_features must not run under privacy=high")


class _WeakLocal:
    n_layers = 32

    def generate(self, prompt: str) -> str:
        _ = prompt
        return "LOCAL_WEAK: " + "x" * 700

    def forward_partial(self, x, *, layers: range):  # pragma: no cover - infer-only tests
        from cos.sigma_split import ToyLocalModel

        return ToyLocalModel().forward_partial(x, layers=layers)


def test_easy_prompt_routes_local() -> None:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyLocalModel, ToyRemoteModel

    inf = SigmaSplitInference(
        ToyLocalModel(),
        remote_endpoint=ToyRemoteModel(),
        gate=SigmaSplitGate(),
        spec_sigma_threshold=0.3,
    )
    r = inf.infer("What is quantum computing?", privacy="normal")
    assert r["location"] == "local"
    assert r["verdict"] == "ACCEPT"
    assert str(r["response"]).startswith("LOCAL:")


def test_hard_prompt_routes_remote() -> None:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyLocalModel, ToyRemoteModel

    long_q = "Derive the full proof of Fermat's theorem. " + ("word " * 80)
    inf = SigmaSplitInference(
        ToyLocalModel(),
        remote_endpoint=ToyRemoteModel(),
        gate=SigmaSplitGate(),
        spec_sigma_threshold=0.3,
    )
    r = inf.infer(long_q, privacy="normal")
    assert r["location"] == "remote"
    assert "REMOTE" in str(r["response"])


def test_privacy_high_forces_local() -> None:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyLocalModel

    inf = SigmaSplitInference(
        ToyLocalModel(),
        remote_endpoint=_BoomRemote(),
        gate=SigmaSplitGate(),
    )
    r = inf.infer("My SSN is 123-45-6789", privacy="high")
    assert r["location"] == "local"
    assert r["reason"] == "privacy"


def test_local_uncertain_falls_back_remote() -> None:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyRemoteModel

    inf = SigmaSplitInference(
        _WeakLocal(),
        remote_endpoint=ToyRemoteModel(),
        gate=SigmaSplitGate(),
        spec_sigma_threshold=0.3,
    )
    r = inf.infer("short", privacy="normal")
    assert r["location"] == "remote"


def test_layer_split_returns_tag() -> None:
    from cos.sigma_split import SigmaSplitGate, SigmaSplitInference, ToyLocalModel, ToyRemoteModel

    inf = SigmaSplitInference(
        ToyLocalModel(),
        remote_endpoint=ToyRemoteModel(),
        gate=SigmaSplitGate(),
    )
    _tensor, tag = inf.layer_split("x" * 520, split_point=8)
    assert tag in ("split", "local_complete", "local_complete_no_remote")


def test_sigma_fleet_register_health_route() -> None:
    from cos.sigma_fleet import SigmaFleet

    f = SigmaFleet()
    f.register("laptop-m4", {"model": "bitnet-2b", "local": True, "avg_sigma": 0.1, "calibration_date": "2026-04-01"})
    f.register("edge-rpi", {"model": "bitnet-500m", "local": True, "avg_sigma": 0.23})
    f.register("cloud-a100", {"model": "llama-70b", "local": False, "avg_sigma": 0.05})
    h = f.fleet_health()
    assert h["total_devices"] == 3
    assert h["active"] == 3
    best = f.route_to_best("any", privacy="high")
    assert best is not None
    assert best["id"] == "laptop-m4"
    assert best["capabilities"].get("local") is True
    f.sync_calibration("laptop-m4", ["edge-rpi"])
    assert f.devices["edge-rpi"].get("sigma_calibration") == "2026-04-01"


def _pytest_env() -> dict:
    return {**os.environ, "PYTHONPATH": str(REPO / "python")}


@pytest.mark.parametrize(
    "argv,needle",
    [
        (["split", "--prompt", "What is quantum computing?", "--privacy", "normal"], '"location": "local"'),
        (["split", "--prompt", "My SSN", "--privacy", "high"], '"reason": "privacy"'),
        (["split", "--layer", "--split-point", "8"], "layer_tag"),
    ],
)
def test_cos_split_cli(argv: list, needle: str) -> None:
    proc = subprocess.run(
        [sys.executable, "-m", "cos", *argv],
        cwd=REPO,
        env=_pytest_env(),
        check=True,
        capture_output=True,
        text=True,
    )
    assert needle in proc.stdout


def test_cos_fleet_cli_health(tmp_path: Path) -> None:
    state = tmp_path / "fleet.json"
    subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "fleet",
            "--register",
            "--device",
            "a",
            "--model",
            "m",
            "--state",
            str(state),
        ],
        cwd=REPO,
        env=_pytest_env(),
        check=True,
        capture_output=True,
        text=True,
    )
    proc = subprocess.run(
        [sys.executable, "-m", "cos", "fleet", "--health", "--state", str(state)],
        cwd=REPO,
        env=_pytest_env(),
        check=True,
        capture_output=True,
        text=True,
    )
    assert "total_devices" in proc.stdout
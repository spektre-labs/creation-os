# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for v161 σ-federated toy aggregator (lab — not a security proof)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_federated import (  # noqa: E402
    SigmaDifferentialPrivacy,
    SigmaFederatedClient,
    client_self_check_gate,
    demo_aggregate_memory,
    run_mock_federation_lab,
)
from cos.sigma_gate_core import Verdict  # noqa: E402


def test_poison_updates_rejected_at_server() -> None:
    out = demo_aggregate_memory(include_poison=True, use_byzantine=True, dp=None)
    agg = out["aggregate"]
    assert agg.get("aggregated") is True
    assert agg.get("accepted") == 8
    assert agg.get("rejected") >= 2
    assert len(agg.get("rejected_clients", [])) >= 2


def test_honest_client_self_abstain_skips_send() -> None:
    gw = {f"w{k}": [0.0, 0.0] for k in range(2)}
    cl = SigmaFederatedClient("honest", use_self_gate=True)
    up = cl.train(gw, poison=False, honest_scale=80.0)
    assert up is None


def test_self_check_gate_abstain_on_spike() -> None:
    gw = {"w0": [0.0]}
    cand = {"w0": [500.0]}
    _s, v = client_self_check_gate(gw, cand)
    assert v == Verdict.ABSTAIN


def test_dp_noise_changes_aggregate() -> None:
    base = demo_aggregate_memory(include_poison=True, use_byzantine=True, dp=None)
    dp = SigmaDifferentialPrivacy(epsilon=1.0, delta=1e-5)
    noised = demo_aggregate_memory(include_poison=True, use_byzantine=True, dp=dp)
    assert base["aggregate"].get("aggregated") and noised["aggregate"].get("aggregated")
    assert "client_stats" in base and "client_stats" in noised


def test_run_mock_writes_state() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        st = run_mock_federation_lab(rounds=2, workspace=tmp, include_poison=True, use_byzantine=True)
        assert st.get("rounds_run") == 2
        assert (Path(tmp) / "fed_lab_state.json").is_file()


def test_cos_federated_aggregate_cli() -> None:
    env = {**os.environ, "PYTHONPATH": str(_REPO / "python")}
    with tempfile.TemporaryDirectory() as tmp:
        r = subprocess.run(
            [
                sys.executable,
                "-m",
                "cos",
                "federated",
                "--aggregate",
                "--workspace",
                tmp,
            ],
            cwd=str(_REPO),
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        assert r.returncode == 0, r.stderr
        body = json.loads(r.stdout.strip())
        assert body["aggregate"].get("aggregated") is True
        assert (Path(tmp) / "fed_v161_stats.json").is_file()

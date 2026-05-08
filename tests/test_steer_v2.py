# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

from cos.sigma_gate import SigmaGate
from cos.steer import SigmaSteer


def test_per_token_steer_logs() -> None:
    gate = SigmaGate()
    st = SigmaSteer(gate=gate)
    tokens = ["alpha", "beta"]
    out = st.per_token_steer(tokens, gate=gate)

    assert out["total_tokens"] == 2
    assert len(out["log"]) == 2
    for i, row in enumerate(out["log"]):
        assert row["position"] == i
        assert row["token"] == tokens[i]
        assert "σ" in row and isinstance(row["σ"], (int, float))
        assert "verdict" in row and isinstance(row["verdict"], str) and row["verdict"]
        assert row["steered"] in (True, False)
        assert "feature_id" in row and "action" in row


def test_branch_point_detection(monkeypatch: pytest.MonkeyPatch) -> None:
    pytest.importorskip("numpy")
    from cos.sae import SigmaSAE

    gate = SigmaGate()
    sae = SigmaSAE(gate=gate, input_dim=8, hidden_dim=32)
    st = SigmaSteer(gate=gate, sae=sae)

    def fake_score(_p: str, t: str) -> tuple[float, str]:
        return (0.88, "ABSTAIN") if t == "risky" else (0.12, "ACCEPT")

    def fake_target(p: str, r: str) -> dict:  # noqa: ARG001
        if r == "risky":
            return {"target": 9, "action": "suppress", "method": "sae"}
        return {"target": None, "method": "no_target_found"}

    monkeypatch.setattr(gate, "score", fake_score)
    monkeypatch.setattr(st, "identify_target", fake_target)

    out = st.per_token_steer(["safe", "risky", "safe"], gate=gate)
    steered = [e for e in out["log"] if e["steered"]]
    assert len(steered) == 1
    assert steered[0]["token"] == "risky"
    assert steered[0]["feature_id"] == 9
    assert out["branch_points"] == steered


def test_steer_rate_calculation(monkeypatch: pytest.MonkeyPatch) -> None:
    pytest.importorskip("numpy")
    from cos.sae import SigmaSAE

    gate = SigmaGate()
    sae = SigmaSAE(gate=gate, input_dim=8, hidden_dim=32)
    st = SigmaSteer(gate=gate, sae=sae)

    seq = ["n1", "n2", "y1", "y2"]

    def fake_score(_p: str, t: str) -> tuple[float, str]:
        return (0.88, "ABSTAIN") if t.startswith("y") else (0.05, "ACCEPT")

    monkeypatch.setattr(gate, "score", fake_score)
    monkeypatch.setattr(
        st,
        "identify_target",
        lambda _p, r: {"target": 1, "action": "suppress", "method": "sae"} if r.startswith("y") else {"target": None, "method": "x"},
    )

    out = st.per_token_steer(seq, gate=gate)
    assert out["steered_tokens"] == 2
    assert out["steer_rate"] == pytest.approx(0.5)


def test_critic_trajectory_accuracy() -> None:
    st = SigmaSteer()
    assert st.critic_trajectory([0.1, 0.2]) == {"analysis": "insufficient data"}

    r = st.critic_trajectory([0.2, 0.15, 0.1])
    assert r["prediction_accuracy"] == 1.0
    assert r["policy_reliable"] is True
    assert r["n_steps"] == 3

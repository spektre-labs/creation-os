# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v146 σ-MoE: σ-gate routing, fallback path, entropy collapse signal, layer blend."""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any

import pytest

from cos.sigma_moe import LabExpert, LabMoEGate, SigmaMoE


def test_three_experts_low_sigma_expert_wins_quantum_prompt() -> None:
    experts = [
        LabExpert(name="bitnet-2b", pattern="weak"),
        LabExpert(name="gemma-2b", pattern="strong"),
        LabExpert(name="llama-8b", pattern="strong"),
    ]
    moe = SigmaMoE(experts, LabMoEGate(), fallback_expert=LabExpert(name="fallback", pattern="strong"))
    out = moe.route("Explain quantum entanglement briefly.")
    assert out.get("verdict") == "ACCEPT"
    assert out.get("expert") == "gemma-2b"
    assert out.get("sigma", 1.0) < 0.1
    assert "entanglement" in str(out.get("response", "")).lower()


def test_all_abstain_uses_fallback() -> None:
    experts = [
        LabExpert(name="a", pattern="abstain"),
        LabExpert(name="b", pattern="abstain"),
    ]
    fb = LabExpert(name="fallback", pattern="strong")
    moe = SigmaMoE(experts, LabMoEGate(), fallback_expert=fb)
    out = moe.route("Explain quantum entanglement briefly.")
    assert out.get("expert") == "fallback"
    assert out.get("verdict") == "ACCEPT"
    assert "entanglement" in str(out.get("response", "")).lower()


def test_expert_health_collapsed_when_imbalanced() -> None:
    experts = [LabExpert(name=f"e{i}", pattern="strong") for i in range(3)]
    moe = SigmaMoE(experts, LabMoEGate())
    moe.expert_stats[0]["uses"] = 1000
    moe.expert_stats[1]["uses"] = 2
    moe.expert_stats[2]["uses"] = 1
    h = moe.expert_health()
    assert h["collapsed"] is True
    assert h["balance"] < 0.3


def test_layer_moe_weighted_mix_numpy() -> None:
    np = pytest.importorskip("numpy")

    class E0:
        def forward(self, h: Any) -> Any:
            return np.array([1.0, 0.0], dtype="float64") + h * 0.01

    class E1:
        def forward(self, h: Any) -> Any:
            return np.array([0.0, 2.0], dtype="float64") + h * 0.02

    moe = SigmaMoE([LabExpert(name="dummy", pattern="strong")], LabMoEGate())
    h = np.zeros(2, dtype="float64")
    mixed = moe.layer_moe(h, [E0(), E1()], top_k=2)
    assert mixed.shape == (2,)
    assert float(np.sum(mixed)) > 0.1


def test_cli_moe_register_json(tmp_path: Path) -> None:
    env = __import__("os").environ.copy()
    root = Path(__file__).resolve().parents[1]
    env["PYTHONPATH"] = str(root / "python") + (":" + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    reg = tmp_path / "reg.json"
    p = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "moe",
            "--register",
            "medical",
            "--model",
            "medical-lora-7b",
            "--registry",
            str(reg),
        ],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert p.returncode == 0, p.stderr
    data = json.loads(p.stdout)
    assert data.get("registered") == "medical"
    assert "medical" in json.loads(reg.read_text(encoding="utf-8"))

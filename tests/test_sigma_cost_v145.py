# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v145 σ-cost: cheapest-first routing, ABSTAIN = zero billable row, per-verdict invoice."""
from __future__ import annotations

import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

from cos.sigma_cost import LabCostGate, SigmaBillingMeter, SigmaCost


def test_route_cheap_model_accept_skips_expensive() -> None:
    order: list[str] = []

    def gen(model: str, prompt: str) -> str:
        order.append(model)
        if model == "bitnet-2b":
            return "4"
        return "maybe 5"

    cost = SigmaCost(LabCostGate(), generate=gen)
    out = cost.route_by_cost("What is 2+2?", models=["bitnet-2b", "llama-8b", "llama-70b"])
    assert out.get("verdict") == "ACCEPT"
    assert out.get("model") == "bitnet-2b"
    assert "llama-70b" not in order
    assert float(out.get("savings_usd") or 0) > 0.0


def test_abstain_returns_zero_billable_cost() -> None:
    def gen(_m: str, _p: str) -> str:
        return "nonsense @@@"

    cost = SigmaCost(LabCostGate(), generate=gen)
    out = cost.route_by_cost("hello", models=["bitnet-2b"])
    assert out.get("verdict") == "ABSTAIN"
    assert out["cost_usd"] == 0.0


def test_billing_meter_invoice_per_user_month() -> None:
    t_ok = datetime(2026, 5, 14, 12, 0, 0, tzinfo=timezone.utc).timestamp()
    t_other = datetime(2026, 4, 30, 12, 0, 0, tzinfo=timezone.utc).timestamp()
    meter = SigmaBillingMeter(price_per_verdict=0.001)
    meter.events = [
        {
            "user_id": "alice",
            "sigma": 0.1,
            "verdict": "ACCEPT",
            "model": "bitnet-2b",
            "billable": True,
            "amount": 0.001,
            "timestamp": t_ok,
        },
        {
            "user_id": "alice",
            "sigma": 0.2,
            "verdict": "RETHINK",
            "model": "bitnet-2b",
            "billable": False,
            "amount": 0.0,
            "timestamp": t_ok,
        },
        {
            "user_id": "alice",
            "sigma": 0.1,
            "verdict": "ACCEPT",
            "model": "bitnet-2b",
            "billable": True,
            "amount": 0.001,
            "timestamp": t_other,
        },
    ]
    inv = meter.invoice("alice", "2026-05")
    assert inv["total_verdicts"] == 1
    assert abs(float(inv["total_amount"]) - 0.001) < 1e-9


def test_cli_cost_route_writes_state(tmp_path: Path) -> None:
    st = tmp_path / "cost.json"
    env = __import__("os").environ.copy()
    root = Path(__file__).resolve().parents[1]
    env["PYTHONPATH"] = str(root / "python") + (":" + env["PYTHONPATH"] if env.get("PYTHONPATH") else "")
    p = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "cost",
            "--route",
            "--prompt",
            "What is 2+2?",
            "--models",
            "bitnet-2b,llama-8b",
            "--state",
            str(st),
        ],
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert p.returncode == 0, p.stderr
    data = json.loads(p.stdout)
    assert data.get("verdict") == "ACCEPT"
    assert st.is_file()

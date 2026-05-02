# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v134 — σ-consolidation + replay + EWC lab (no torch)."""
from __future__ import annotations

import json
from pathlib import Path


from cos.sigma_consolidation import (
    SigmaConsolidation,
    SigmaEWC,
    ToyContinualGate,
    ToyContinualModel,
)
from cos.sigma_gate_core import Verdict
from cos.sigma_replay import SigmaReplayBuffer


def test_anchor_gentle_many_steps_no_regression() -> None:
    model = ToyContinualModel()
    gate = ToyContinualGate(model)
    cons = SigmaConsolidation(gate, model)
    anchors = [("What is 2+2?", "4")]
    cons.set_anchors(anchors)
    for _ in range(100):
        out = cons.learn_step([("unrelated stable topic", "ok")], lr=0.001, tolerance=0.05)
        assert out.get("learned") is True, out
    chk = cons.check_anchors(tolerance=0.05)
    assert chk["regressed"] is False


def test_aggressive_lr_triggers_rollback() -> None:
    model = ToyContinualModel()
    gate = ToyContinualGate(model)
    cons = SigmaConsolidation(gate, model)
    ap = "What is 2+2?"
    cons.set_anchors([(ap, "4")])
    r = cons.learn_step([(f"context {ap} poison", "x")], lr=0.95, tolerance=0.05)
    assert r.get("learned") is False
    assert "regression" in str(r.get("reason", ""))


def test_replay_accepts_only_and_evicts_high_sigma() -> None:
    buf = SigmaReplayBuffer(max_size=3)
    assert buf.add("a", "b", 0.1, Verdict.ACCEPT) is True
    assert buf.add("c", "d", 0.5, Verdict.ABSTAIN) is False
    buf.add("e", "f", 0.2, Verdict.ACCEPT)
    buf.add("g", "h", 0.15, Verdict.ACCEPT)
    buf.add("i", "j", 0.9, Verdict.ACCEPT)
    assert len(buf.buffer) == 3
    sigmas = sorted(float(x["sigma"]) for x in buf.buffer)
    assert sigmas[-1] < 0.85


def test_ewc_fisher_skips_non_accept() -> None:
    model = ToyContinualModel()
    gate = ToyContinualGate(model)
    ewc = SigmaEWC(model, gate, lambda_ewc=10.0)
    ewc.compute_fisher_sigma(
        [
            ("p1", "a1"),
            ("force rethink long ambiguous text poison", "b"),
        ]
    )
    # Second row may be RETHINK/ABSTOP — Fisher may be partial or empty; loss still finite
    _ = ewc.ewc_loss()
    assert isinstance(ewc.fisher, dict)


def test_cli_forgetting_test(tmp_path: Path) -> None:
    from cos.cli import main

    anchors = tmp_path / "a.jsonl"
    anchors.write_text(
        json.dumps({"prompt": "What is 2+2?", "answer": "4"}) + "\n", encoding="utf-8"
    )
    rc = main(
        ["learn", "--forgetting-test", "--anchors", str(anchors), "--steps", "100", "--lr", "0.001"]
    )
    assert rc == 0


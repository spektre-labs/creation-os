# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

import pytest

from cos.eval.lm_eval_bridge import LMEvalBridge, _verdict_from_sigma


def test_score_results_missing_file(tmp_path: Path) -> None:
    b = LMEvalBridge()
    out = b.score_results(tmp_path / "nope.json")
    assert out.get("error") == "file not found"


def test_score_results_parses_tasks(tmp_path: Path) -> None:
    sample = {
        "model": "m1",
        "results": {
            "arc": {"acc,none": 0.8},
            "x": {"acc_norm,none": 0.6},
        },
    }
    p = tmp_path / "r.json"
    p.write_text(json.dumps(sample), encoding="utf-8")
    b = LMEvalBridge()
    out = b.score_results(p)
    assert "error" not in out
    assert out["tasks"]["arc"]["σ"] == pytest.approx(0.2, abs=0.01)
    assert out["tasks"]["x"]["verdict"] == "RETHINK"
    assert out["avg_σ"] > 0


def test_verdict_thresholds() -> None:
    assert _verdict_from_sigma(0.1) == "ACCEPT"
    assert _verdict_from_sigma(0.2) == "RETHINK"
    assert _verdict_from_sigma(0.9) == "ABSTAIN"


def test_generate_eval_config_lists_tasks() -> None:
    b = LMEvalBridge()
    cfg = b.generate_eval_config()
    assert "truthfulqa_mc2" in cfg["tasks"]
    assert "lm_eval" in cfg["command"]
    assert "post_process" in cfg

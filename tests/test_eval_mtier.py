# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.eval.mtier import mtier_payload, print_mtier  # noqa: E402
from cos.sigma_gate import SigmaGate  # noqa: E402


def test_mtier_payload_includes_halueval_negative() -> None:
    p = mtier_payload()
    halu = next(x for x in p["results"] if x["benchmark"] == "HaluEval")
    assert halu["value"] == 0.514
    assert halu["status"] == "fail"


def test_mtier_payload_truthfulqa_saturation_note() -> None:
    p = mtier_payload()
    tq = next(x for x in p["results"] if "TruthfulQA" in x["benchmark"])
    assert tq["value"] == 0.982
    assert "saturated" in tq["note"].lower()


def test_print_mtier_runs(capsys) -> None:
    print_mtier()
    out = capsys.readouterr().out
    assert "HaluEval" in out
    assert "0.514" in out


def test_sigma_gate_import_anchor() -> None:
    assert SigmaGate is not None

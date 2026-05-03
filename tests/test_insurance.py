# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.insurance`."""
from __future__ import annotations

from cos.insurance import SigmaInsurance
from cos.sigma_gate import SigmaGate


def test_risk_score_bounds() -> None:
    ins = SigmaInsurance()
    r = ins.risk_score(SigmaGate(), {"sigma_avg": 0.2, "AUROC": 0.9, "evidence_ladder_negatives": True}, {})
    assert 0 <= r["risk_score_0_100"] <= 100


def test_policy_requirements() -> None:
    p = SigmaInsurance.policy_requirements(20)
    assert "requirements" in p and len(p["requirements"]) >= 2


def test_incident_report() -> None:
    r = SigmaInsurance.incident_report({"id": "e1", "description": "x"}, 0.7, "ABSTAIN")
    assert r["verdict_at_time"] == "ABSTAIN"


def test_actuarial_data() -> None:
    ins = SigmaInsurance()
    r = ins.actuarial_data([{"sigma": 0.1}, {"sigma": 0.3}])
    assert r["n"] == 2 and r["mean_sigma"] > 0


def test_certificate_disclaimer() -> None:
    ins = SigmaInsurance()
    c = ins.certificate(SigmaGate(), {"sigma_avg": 0.2}, compliance={})
    assert "disclaimer" in c and "Spektre" in c["label"]

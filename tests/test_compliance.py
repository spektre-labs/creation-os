# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.compliance`."""
from __future__ import annotations

from cos.compliance import SigmaCompliance
from cos.sigma_gate import SigmaGate


def test_art9_risk_report_generated() -> None:
    c = SigmaCompliance(SigmaGate())
    r = c.art9_risk_management(
        c.gate,
        {"sample_pairs": [{"prompt": "a", "response": "b"}, {"prompt": "c", "response": ""}]},
    )
    assert r["article"] == 9 and "sigma_mean" in r


def test_art11_tech_doc_contains_required_fields() -> None:
    doc = SigmaCompliance.art11_technical_doc(None, {"training_data_summary": "x"}, [{"name": "p"}])
    assert "architecture" in doc and "evaluation" in doc and "limitations" in doc


def test_art12_logs_retained() -> None:
    r = SigmaCompliance.art12_record_keeping([{"verdict": "ACCEPT"}, {"verdict": "ABSTAIN"}])
    assert r["records"] == 2 and r["verdict_histogram"]["ABSTAIN"] == 1


def test_art13_deployer_info_complete() -> None:
    c = SigmaCompliance(SigmaGate())
    r = c.art13_transparency(c.gate)
    assert "deployer_summary" in r and ("τ" in r["deployer_summary"] or "accept" in r["deployer_summary"].lower())


def test_art14_rethink_requires_human() -> None:
    r = SigmaCompliance.art14_human_oversight(None)
    assert "review" in r["RETHINK"].lower()


def test_art14_abstain_blocks_action() -> None:
    r = SigmaCompliance.art14_human_oversight(None)
    assert "block" in r["ABSTAIN"].lower() or "human" in r["ABSTAIN"].lower()


def test_art15_accuracy_includes_negatives() -> None:
    r = SigmaCompliance.art15_accuracy({"AUROC": 0.8, "evidence_ladder_negatives": 12})
    assert r["evidence_ladder_negatives_documented"] is True and r["evidence_ladder_negatives"] == 12


def test_art50_machine_readable_mark() -> None:
    s = SigmaCompliance.art50_transparency_mark("hello", None)
    assert "machine_generated" in s or "ai_disclosure" in s


def test_art86_explanation_generated() -> None:
    c = SigmaCompliance(SigmaGate())
    r = c.art86_right_to_explanation("p", "r", 0.4, "RETHINK")
    assert r["article"] == 86 and "explanation" in r


def test_full_report_all_articles() -> None:
    c = SigmaCompliance(SigmaGate())
    rep = c.full_report(
        {
            "sample_pairs": [{"prompt": "1", "response": "2"}],
            "audit_log": [],
            "bench_results": {"evidence_ladder_negatives": 3},
        },
    )
    for k in ("art9", "art11", "art12", "art13", "art14", "art15", "art50", "art86", "deadline"):
        assert k in rep
    assert rep["deadline"]["days_remaining"] >= 0

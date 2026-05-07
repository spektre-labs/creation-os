# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for L7 lab σ commitments in ``cos.zkp`` (SHA-256 payload binding; not a succinct SNARK)."""
from __future__ import annotations

import json
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate import SigmaGate  # noqa: E402
from cos.zkp import SigmaCommitment, SigmaConstitution  # noqa: E402


def test_commit_creates_hash() -> None:
    zkp = SigmaCommitment()
    record = zkp.commit("p", "r", 0.42, "ACCEPT", gate_version="1.0.0")
    assert len(record["commitment"]) == 64
    assert record["payload"]
    assert record["sigma"] == 0.42


def test_verify_valid_commitment() -> None:
    zkp = SigmaCommitment()
    record = zkp.commit("What", "Answer", 0.33, "RETHINK")
    assert zkp.verify(record) is True


def test_verify_tampered_fails() -> None:
    zkp = SigmaCommitment()
    record = zkp.commit("p", "r", 0.25, "ABSTAIN")
    body = json.loads(record["payload"])
    body["sigma"] = 0.99
    bad = {**record, "payload": json.dumps(body, sort_keys=True)}
    assert zkp.verify(bad) is False


def test_proof_receipt_format() -> None:
    zkp = SigmaCommitment()
    record = zkp.commit("a", "b", 0.5, "ACCEPT")
    text = zkp.proof_receipt(record)
    assert "PROOF RECEIPT" in text
    assert "Commitment:" in text
    assert "σ:" in text
    assert "Verdict:" in text
    assert "Verified: True" in text


def test_batch_verify_all_valid() -> None:
    zkp = SigmaCommitment()
    zkp.commit("a", "b", 0.1, "ACCEPT")
    zkp.commit("c", "d", 0.2, "RETHINK")
    batch = zkp.batch_verify()
    assert batch["total"] == 2
    assert batch["all_valid"] is True
    assert all(r["verified"] for r in batch["results"])


def test_constitution_check_compliant() -> None:
    const = SigmaConstitution()
    gate = SigmaGate()
    result = const.check(gate)
    assert result["compliant"] is True
    assert result["rules"] == 10
    assert not result["violations"]


def test_constitution_tamper_detected() -> None:
    orig = SigmaConstitution.CONSTITUTION
    probe = SigmaConstitution()
    assert probe.tampered() is False
    try:
        SigmaConstitution.CONSTITUTION = tuple(orig) + ("rogue declarative line",)
        assert probe.tampered() is True
    finally:
        SigmaConstitution.CONSTITUTION = orig


def test_constitution_threshold_violation() -> None:
    class BadGate:
        threshold_accept = 0.9
        threshold_abstain = 0.5

        def score(self, _p: str, _r: str):  # noqa: ANN001
            return 0.5, "ACCEPT"

    const = SigmaConstitution()
    result = const.check(BadGate())
    assert result["compliant"] is False
    assert any("threshold" in v for v in result["violations"])

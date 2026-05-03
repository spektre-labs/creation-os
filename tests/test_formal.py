# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.formal import SigmaFormal


def test_verify_verdict() -> None:
    formal = SigmaFormal()
    result = formal.verify_verdict("What is 2+2?", "4")
    assert result["properties"]["all_hold"]
    assert result["deterministic"]


def test_properties_sigma_range() -> None:
    formal = SigmaFormal()
    result = formal.verify_verdict("test", "hello")
    checks = result["properties"]["checks"]
    sigma_check = next(c for c in checks if c["property"] == "sigma_range")
    assert sigma_check["holds"]


def test_batch_verify() -> None:
    formal = SigmaFormal()
    result = formal.batch_verify(
        [
            ("a", "b"),
            ("c", "d"),
        ]
    )
    assert result["batch_size"] == 2
    assert isinstance(result["all_verified"], bool)


def test_proof_hash() -> None:
    formal = SigmaFormal()
    r1 = formal.verify_verdict("test", "hello")
    r2 = formal.verify_verdict("test", "hello")
    assert r1["proof_hash"] == r2["proof_hash"]


def test_lean_not_installed() -> None:
    formal = SigmaFormal(lean_path="/nonexistent/lean")
    result = formal.verify_verdict("test", "hello")
    assert result["lean_check"]["status"] in ("lean_not_installed", "error")

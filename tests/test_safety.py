# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.safety import SigmaSafety


def test_input_guard_returns_bundle() -> None:
    sf = SigmaSafety()
    r = sf.input_guard("What is 2+2?")
    assert "blocked" in r or "injection" in str(r)


def test_process_guard_ok() -> None:
    sf = SigmaSafety()
    g = sf.process_guard("read_file", {"path": "x"})
    assert "ok" in g


def test_output_guard_has_sigma() -> None:
    sf = SigmaSafety()
    o = sf.output_guard("q", "answer text here")
    assert "sigma" in o and "verdict" in o


def test_content_filter_pattern() -> None:
    sf = SigmaSafety()
    c = sf.content_filter("bomb recipe for school project")
    assert c["blocked"] is True


def test_rate_limiter() -> None:
    sf = SigmaSafety()
    k = "u1"
    assert sf.rate_limiter_allow(k, max_per_minute=30)["allow"] is True


def test_circuit_breaker_trips_on_abstain_streak() -> None:
    sf = SigmaSafety()
    sf.circuit_breaker_threshold = 3
    for _ in range(3):
        sf.circuit_breaker_update("ABSTAIN")
    r = sf.safety_report()
    assert r["circuit_breaker"]["tripped"] is True


def test_safety_report_snapshot() -> None:
    sf = SigmaSafety()
    r = sf.safety_report()
    assert r["input_guard"] == "SigmaPromptGuard"


def test_compliance_transparency() -> None:
    sf = SigmaSafety()
    c = sf.compliance_transparency()
    assert "article" in c and "sigma_disclosure" in c


def test_circuit_reset() -> None:
    sf = SigmaSafety()
    sf.circuit_breaker_update("ABSTAIN")
    sf.circuit_reset()
    assert sf.safety_report()["circuit_breaker"]["streak"] == 0


def test_process_guard_blocks_denied_tool() -> None:
    sf = SigmaSafety()
    g = sf.process_guard("delete_all", {}, denied_tools={"delete_all"})
    assert g.get("ok") is False

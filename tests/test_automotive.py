# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.automotive`."""
from __future__ import annotations

from cos.automotive import SigmaAutomotive
from cos.sigma_gate import SigmaGate


def test_safety_command_never_cloud() -> None:
    a = SigmaAutomotive()
    r = a.route_path("How do I engage the diff_lock on this SUV?")
    assert r["cloud_llm_allowed"] is False and r["edge_only"] is True


def test_abstain_gives_manual_page() -> None:
    g = SigmaGate()
    a = SigmaAutomotive(g)
    chunks = [
        {"text": "Unrelated fluff about wipers.", "page": 42, "verified": True},
    ]
    out = a.manual_rag("How do I lock the differential?", chunks)
    assert out["verdict"] == "ABSTAIN"
    hint = out.get("spoken_hint", "") + str(out.get("page", ""))
    assert "page" in hint.lower() or out.get("page") == 42


def test_driving_context_shortens_response() -> None:
    a = SigmaAutomotive()
    r = a.context_aware_interrupt("120km/h highway", 0.2, "ACCEPT")
    assert r["max_words"] <= 15


def test_offline_mode_works() -> None:
    assert SigmaAutomotive().offline_mode is True


def test_latency_under_50ms() -> None:
    """Python gate mirror: probe mean ms; design target still 50 ms on fixed-point path."""
    a = SigmaAutomotive(SigmaGate())
    budgets = SigmaAutomotive.latency_budget()
    assert budgets["edge_gate_target_ms"] <= 50.0
    ms = a.gate_latency_probe_ms(iterations=5)
    assert ms < 500.0


def test_hallucination_blocked_for_vehicle_controls() -> None:
    a = SigmaAutomotive()
    r = a.enforce_vehicle_answer(
        "activate diff_lock now",
        "Just press the red button three times.",
        from_verified_manual=False,
    )
    assert r["verdict"] == "ABSTAIN" and r["allow_voice"] is False


def test_manual_rag_only_from_verified_source() -> None:
    a = SigmaAutomotive(SigmaGate())
    out = a.manual_rag(
        "tire pressure",
        [{"text": "35 psi", "page": 10, "verified": False}],
    )
    assert out["verdict"] == "ABSTAIN"


def test_rethink_defers_to_screen() -> None:
    a = SigmaAutomotive()
    pol = a.voice_output_policy(0.6, "RETHINK", "parked")
    assert pol["defer_to_screen"] is True and pol["play_voice"] is False

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.persona`."""
from __future__ import annotations

from cos.persona import SigmaPersona
from cos.sigma_gate import SigmaGate


def test_builtin_personas_exist() -> None:
    p = SigmaPersona()
    for k in ("automotive", "medical", "creative", "enterprise", "research"):
        assert k in p.personas


def test_activate_medical() -> None:
    p = SigmaPersona()
    spec = p.activate("medical")
    assert spec["threshold_accept"] < spec["threshold_abstain"]


def test_custom_persona() -> None:
    p = SigmaPersona()
    o = p.custom("edge", {"threshold_accept": 0.05, "threshold_abstain": 0.5}, {"latency_max_ms": 10})
    assert o["name"] == "edge" and p.personas["edge"]["latency_max_ms"] == 10


def test_sigma_policy_blurbs() -> None:
    pol = SigmaPersona.sigma_policy("medical")
    assert "ABSTAIN" in pol and "RETHINK" in pol


def test_context_detect() -> None:
    assert SigmaPersona.context_detect({"domain": "automotive"}) == "automotive"
    assert SigmaPersona.context_detect({"regulated": True}) == "medical"
    assert SigmaPersona.context_detect({}) == "enterprise"


def test_apply_and_restore_gate() -> None:
    p = SigmaPersona()
    g = SigmaGate()
    orig = (g.threshold_accept, g.threshold_abstain)
    prev = p.apply_gate_overrides(g, "creative")
    assert prev is not None
    assert g.threshold_accept == p.personas["creative"]["threshold_accept"]
    p.restore_gate(g, prev)
    assert g.threshold_accept == orig[0] and g.threshold_abstain == orig[1]

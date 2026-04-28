# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.sigma_a2a_card``."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_a2a_card import (  # noqa: E402
    build_sigma_verifier_agent_card,
)


def test_build_card_omits_measured_fields_by_default() -> None:
    card = build_sigma_verifier_agent_card(
        agent_id="creation-os-sigma",
        name="sigma Verifier",
        description="Cognitive interrupt for LLM outputs",
        endpoint="https://example.test/.well-known/agent.json",
    )
    sp = card["sigma_profile"]
    assert "auroc" not in sp
    assert "calibration_mean" not in sp
    assert sp["kernel_size_bytes"] == 12
    assert sp["verdict_types"] == ["ACCEPT", "RETHINK", "ABSTAIN"]
    assert "lsd" in sp["signals"]
    assert card["protocols"] == ["mcp", "a2a"]


def test_build_card_includes_optional_measured() -> None:
    card = build_sigma_verifier_agent_card(
        agent_id="x",
        name="x",
        description="d",
        endpoint="https://e/",
        measured_auroc=0.5,
        measured_calibration_mean=0.9,
    )
    assert card["sigma_profile"]["auroc"] == 0.5
    assert card["sigma_profile"]["calibration_mean"] == 0.9


def test_endpoint_required() -> None:
    try:
        build_sigma_verifier_agent_card(
            agent_id="a",
            name="n",
            description="d",
            endpoint="   ",
        )
    except ValueError:
        return
    raise AssertionError("expected ValueError")

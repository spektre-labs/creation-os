# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from dataclasses import FrozenInstanceError

import pytest

from cos.config import DEFAULT_CONFIG, SigmaConfig


def test_default_thresholds() -> None:
    assert DEFAULT_CONFIG.threshold_accept == 0.15
    assert DEFAULT_CONFIG.threshold_abstain == 0.85


def test_verdict_accept() -> None:
    c = SigmaConfig()
    assert c.verdict(0.05) == "ACCEPT"


def test_verdict_rethink() -> None:
    c = SigmaConfig()
    assert c.verdict(0.50) == "RETHINK"


def test_verdict_abstain() -> None:
    c = SigmaConfig()
    assert c.verdict(0.90) == "ABSTAIN"


def test_frozen_immutable() -> None:
    c = SigmaConfig()
    with pytest.raises(FrozenInstanceError):
        c.threshold_accept = 0.5  # type: ignore[misc]


def test_with_thresholds() -> None:
    c = SigmaConfig()
    c2 = c.with_thresholds(accept=0.2)
    assert c.threshold_accept == 0.15
    assert c2.threshold_accept == 0.2


def test_invalid_thresholds_raises() -> None:
    with pytest.raises(ValueError):
        SigmaConfig(threshold_accept=0.9, threshold_abstain=0.1)


def test_persona_automotive() -> None:
    c = SigmaConfig.from_persona("automotive")
    assert c.threshold_accept == 0.10
    assert c.threshold_abstain == 0.50

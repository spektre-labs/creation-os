# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.omega import N_PHASES, OmegaLoop, PHASE_NAMES  # noqa: E402


def test_omega_loop_one_turn_has_14_phases() -> None:
    h = OmegaLoop().run("test goal", max_turns=1)
    assert len(h) == 1
    assert len(h[0]["phases"]) == N_PHASES == len(PHASE_NAMES)


def test_omega_loop_respects_max_turns() -> None:
    h = OmegaLoop().run("g", max_turns=3)
    assert len(h) <= 3

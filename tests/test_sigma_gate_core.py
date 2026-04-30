# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Unit tests for ``cos.sigma_gate_core`` (Python mirror of ``sigma_gate.h``)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate_core import (  # noqa: E402
    K_CRIT,
    Q16,
    SIGMA_HALF,
    SigmaState,
    Verdict,
    sigma_gate,
    sigma_q16,
    sigma_update,
    sigma_update_q16,
)


def test_abstain_when_k_eff_below_crit() -> None:
    s = SigmaState(sigma=sigma_q16(0.5), d_sigma=sigma_q16(-0.1), k_eff=sigma_q16(0.05))
    assert sigma_gate(s) == Verdict.ABSTAIN


def test_rethink_when_d_sigma_positive_and_k_ok() -> None:
    s = SigmaState(sigma=sigma_q16(0.4), d_sigma=sigma_q16(0.05), k_eff=sigma_q16(0.5))
    assert sigma_gate(s) == Verdict.RETHINK


def test_accept_when_d_sigma_non_positive_and_k_ok() -> None:
    s = SigmaState(sigma=sigma_q16(0.4), d_sigma=0, k_eff=sigma_q16(0.5))
    assert sigma_gate(s) == Verdict.ACCEPT
    s2 = SigmaState(sigma=sigma_q16(0.4), d_sigma=sigma_q16(-0.01), k_eff=sigma_q16(0.5))
    assert sigma_gate(s2) == Verdict.ACCEPT


def test_sigma_update_k_eff_matches_reference() -> None:
    st = SigmaState()
    sigma_update_q16(st, sigma_q16(0.3), sigma_q16(0.8))
    assert st.sigma == sigma_q16(0.3)
    assert st.d_sigma == sigma_q16(0.3)
    expected_k = ((Q16 - st.sigma) * sigma_q16(0.8)) >> 16
    assert st.k_eff == expected_k
    assert sigma_gate(st) == Verdict.RETHINK


def test_python_matches_c_reference_vectors() -> None:
    """Cross-check a few integer paths against the C header semantics."""
    assert K_CRIT == int(0.127 * Q16)
    assert SIGMA_HALF == Q16 // 2
    st = SigmaState(sigma=0, d_sigma=0, k_eff=Q16)
    sigma_update_q16(st, int(0.25 * Q16), int(0.9 * Q16))
    assert st.k_eff == ((Q16 - st.sigma) * int(0.9 * Q16)) >> 16

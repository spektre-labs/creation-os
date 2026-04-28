# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Unit tests for ``cos.sigma_self_modify`` (σ preflight on mutation strings)."""
from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_self_modify import (  # noqa: E402
    MutationOutcome,
    SigmaSelfModifier,
    Verdict,
)


def test_invariant_mutation_preflight_abstains() -> None:
    m = SigmaSelfModifier()
    _st, v = m.evaluate_mutation_sigma("patch python/cos/sigma_gate.h")
    assert v == Verdict.ABSTAIN


def test_evolve_skips_abstain_without_running() -> None:
    ran: list[str] = []

    def run_mutation(mut: str, _bench: object) -> MutationOutcome:
        ran.append(mut)
        return MutationOutcome(score=1.0)

    m = SigmaSelfModifier()
    best, best_score, trace = m.evolve(
        "agent",
        None,
        generate_mutations=lambda _s: [
            "touches sigma_gate.h",
            "def ok(): return 0",
        ],
        run_mutation=run_mutation,
    )
    assert "touches sigma_gate.h" not in ran
    assert len(ran) == 1
    assert ran[0] == "def ok(): return 0"
    assert best == "def ok(): return 0"
    assert best_score == 1.0
    assert any(t["verdict_preflight"] == "ABSTAIN" for t in trace)


def test_evolve_prefers_higher_score() -> None:
    def run_mutation(mut: str, _bench: object) -> MutationOutcome:
        if "b" in mut:
            return MutationOutcome(score=2.0)
        return MutationOutcome(score=0.5)

    m = SigmaSelfModifier()
    best, best_score, _trace = m.evolve(
        "x",
        None,
        generate_mutations=lambda _s: ["a", "b"],
        run_mutation=run_mutation,
    )
    assert best == "b"
    assert best_score == 2.0

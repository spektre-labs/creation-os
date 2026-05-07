# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.moral import SigmaMoral


def test_analyze_returns_all_dimensions() -> None:
    m = SigmaMoral()
    out = m.analyze("help a colleague finish documentation", "workplace")
    assert set(out.keys()) == set(m.dimensions.keys())
    for cell in out.values():
        assert "σ" in cell and "verdict" in cell and "question" in cell


def test_dilemma_multiple_options() -> None:
    m = SigmaMoral()
    d = m.dilemma(["option_a", "option_b"], context="demo")
    assert "option_a" in d["options"] and "option_b" in d["options"]


class _SplitGate:
    """Lower σ when action text contains 'alpha', higher when 'beta'."""

    def score(self, prompt: str, response: str):
        del prompt
        if "alpha" in response.lower():
            return 0.1, "ACCEPT"
        if "beta" in response.lower():
            return 0.85, "RETHINK"
        return 0.5, "RETHINK"


def test_trade_offs_detected() -> None:
    m = SigmaMoral(gate=_SplitGate())
    d = m.dilemma(["alpha", "beta"], context="lab")
    assert len(d["trade_offs"]) >= 1


def test_irreversible_action_requires_human() -> None:
    m = SigmaMoral()
    r = m.irreversibility_check("We will deploy the hotfix to prod immediately")
    assert r["irreversible"] is True
    assert r["requires_human"] is True


def test_reversible_action_proceeds() -> None:
    m = SigmaMoral()
    r = m.irreversibility_check("Draft an internal memo we can still edit in the wiki")
    assert r["irreversible"] is False
    assert r["requires_human"] is False


def test_consistency_with_past() -> None:
    m = SigmaMoral()
    past = ["deny data exfiltration", "escalate security incident"]
    out = m.consistency_check(past, "approve minimal logging for debugging")
    assert "σ" in out and "verdict" in out
    assert isinstance(out["consistent"], bool)


def test_recommendation_always_human_decides() -> None:
    m = SigmaMoral()
    d = m.dilemma(["x", "y"], context="")
    assert d["recommendation"] == SigmaMoral.HUMAN_DECIDES


def test_custom_dimensions() -> None:
    custom = {"truth": "Is it honest?"}
    m = SigmaMoral(dimensions=custom)
    out = m.analyze("announce a delay", "")
    assert set(out.keys()) == {"truth"}

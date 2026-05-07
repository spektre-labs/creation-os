# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import random

from cos.agency import SigmaAgency


class _BiasGate:
    def score(self, context: str, option: str):
        del context
        o = str(option).lower()
        if o == "good":
            return 0.05, "ACCEPT"
        if o == "bad":
            return 0.95, "ABSTAIN"
        return 0.5, "RETHINK"


def test_choose_returns_option() -> None:
    random.seed(0)
    ag = SigmaAgency(gate=_BiasGate())
    r = ag.choose(["good", "bad"], context="pick")
    assert r["chosen"] in ("good", "bad")
    assert "probability" in r
    assert "alternatives" in r


def test_choose_low_sigma_more_likely() -> None:
    random.seed(2)
    ag = SigmaAgency(gate=_BiasGate())
    n = 400
    goods = sum(ag.choose(["good", "bad"], temperature=0.2)["chosen"] == "good" for _ in range(n))
    assert goods > n * 0.75


def test_temperature_affects_randomness() -> None:
    random.seed(10)
    ag_cold = SigmaAgency(gate=_BiasGate())
    n = 300
    cold_good = sum(ag_cold.choose(["good", "bad"], temperature=0.15)["chosen"] == "good" for _ in range(n))
    random.seed(11)
    ag_hot = SigmaAgency(gate=_BiasGate())
    hot_good = sum(ag_hot.choose(["good", "bad"], temperature=8.0)["chosen"] == "good" for _ in range(n))
    assert cold_good > hot_good


class _StressGate:
    def score(self, p: str, r: str):
        del p
        if "HIGH_SIGMA_OPTION" in str(r):
            return 0.88, "RETHINK"
        return 0.12, "ACCEPT"


def test_counterfactual_regret() -> None:
    ag = SigmaAgency(gate=_StressGate())
    o = ag.counterfactual("HIGH_SIGMA_OPTION", "LOW_STRESS_OPTION")
    assert o["regret"] > 0


def test_counterfactual_relief() -> None:
    ag = SigmaAgency(gate=_StressGate())
    o = ag.counterfactual("LOW_STRESS_OPTION", "HIGH_SIGMA_OPTION")
    assert o["relief"] > 0


class _LowGate:
    def score(self, p: str, r: str):
        del p, r
        return 0.08, "ACCEPT"


def test_autonomy_score_high_when_low_sigma() -> None:
    ag = SigmaAgency(gate=_LowGate())
    ag.choose(["a", "b"], temperature=0.5)
    ag.choose(["a", "b"], temperature=0.5)
    assert ag.autonomy_score() > 0.8


def test_decision_history_accumulates() -> None:
    ag = SigmaAgency(gate=_BiasGate())
    assert len(ag.decision_history) == 0
    ag.choose(["x", "y"], temperature=1.0)
    ag.choose(["x", "y"], temperature=1.0)
    assert len(ag.decision_history) == 2

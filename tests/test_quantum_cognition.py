# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.theory.quantum_cognition import CognitiveState, QuantumCognition  # noqa: E402
from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK  # noqa: E402


class _FlatGate:
    def __init__(self, sigma: float, verdict: str) -> None:
        self._s = float(sigma)
        self._v = verdict

    def score(self, prompt: str, option: str):  # noqa: ARG002
        return (self._s, self._v)


class _OrderGate:
    """Deterministic order sensitivity via prompt tags (for unit tests)."""

    def score(self, prompt: str, option: str) -> tuple[float, str]:
        p = str(prompt)
        o = str(option)
        if "after_a:red" in p:
            return (0.05, ACCEPT) if o == "p2" else (0.7, ABSTAIN)
        if "after_b:p1" in p:
            return (0.05, ACCEPT) if o == "blue" else (0.7, ABSTAIN)
        if "order_first:" in p and "QA" in p:
            return (0.2, ACCEPT) if o == "red" else (0.5, ABSTAIN)
        if "order_first:" in p and "QB" in p:
            return (0.2, ACCEPT) if o == "p1" else (0.5, ABSTAIN)
        return (0.99, ABSTAIN)


class _DisjunctionGate:
    def score(self, prompt: str, option: str):  # noqa: ARG002
        if "disjunction_unknown" in str(prompt):
            return (0.4, RETHINK)
        if "disjunction_known_outcome" in str(prompt):
            return (0.85, ABSTAIN)
        return (0.5, RETHINK)


class _CompGate:
    def score(self, prompt: str, target: str):  # noqa: ARG002
        p = str(prompt).lower()
        if "after" in p:
            return (0.88, ABSTAIN)
        if "likability" in p:
            return (0.15, ACCEPT)
        return (0.25, ACCEPT)


def test_superpose_creates_state() -> None:
    st = QuantumCognition(gate=_FlatGate(0.1, ACCEPT)).superpose(["go", "stay"])
    assert not st.collapsed
    assert len(st.options) == 2


def test_measure_collapses() -> None:
    st = CognitiveState(["x", "y"])
    r = st.measure(_FlatGate(0.1, ACCEPT))
    assert st.collapsed is True
    assert r["collapsed_to"] in ("x", "y")
    assert "verdict" in r


def test_measure_is_irreversible() -> None:
    g = _FlatGate(0.2, ACCEPT)
    st = CognitiveState(["a", "b"])
    r1 = st.measure(g)
    r2 = st.measure(g)
    assert r2.get("already_collapsed") is True
    assert r2["collapsed_to"] == r1["collapsed_to"]


def test_interference_modifies_amplitudes() -> None:
    s1 = CognitiveState(["a", "b"], [0.5, 0.5])
    s2 = CognitiveState(["a", "b"], [0.3, 0.7])
    before = list(s1.amplitudes)
    s1.interfere(s2)
    assert s1.amplitudes != before


def test_decide_returns_verdict() -> None:
    qc = QuantumCognition(gate=_FlatGate(0.05, ACCEPT))
    r = qc.decide(["keep", "drop"], context="trial")
    assert r["verdict"] == ACCEPT
    assert len(qc.measurement_history) == 1


def test_order_effect_non_commutative() -> None:
    qc = QuantumCognition(gate=_OrderGate())
    out = qc.order_effect(
        "QA",
        "QB",
        ["red", "blue"],
        ["p1", "p2"],
    )
    assert out["order_matters"] is True
    assert out["order_1"]["first"]["collapsed_to"] == "red"
    assert out["order_2"]["first"]["collapsed_to"] == "p1"


def test_disjunction_effect_interference() -> None:
    qc = QuantumCognition(gate=_DisjunctionGate())
    out = qc.disjunction_effect(["open", "closed"], known_outcome="open")
    assert out["interference"] > 0.1
    assert out["classical_violated"] is True


def test_complementarity_disturbance() -> None:
    qc = QuantumCognition(gate=_CompGate())
    c = qc.complementarity("honesty", "likability", "candidate-A")
    assert c["disturbance"] > 0.1
    assert c["complementary"] is True

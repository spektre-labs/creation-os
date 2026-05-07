# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.conscious import SigmaConscious


class _FlatGate:
    def score(self, p: str, r: str):
        del p, r
        return 0.42, "ACCEPT"


class _OscGate:
    def __init__(self) -> None:
        self._n = 0

    def score(self, p: str, r: str):
        del p, r
        self._n += 1
        return (0.15 if self._n % 2 == 1 else 0.88, "RETHINK")


def test_sigma_meta_initial() -> None:
    sc = SigmaConscious(gate=_FlatGate())
    assert sc.σ_meta() == 0.5


def test_predict_own_sigma_records() -> None:
    sc = SigmaConscious(gate=_FlatGate())
    sc.predict_own_σ("p", "r")
    assert len(sc.predictions) == 1
    assert len(sc.σ_history) == 1


def test_good_self_knowledge_low_gap() -> None:
    sc = SigmaConscious(gate=_FlatGate())
    out = sc.predict_own_σ("a", "b")
    assert out["self_knowledge"] == "good"


def test_poor_self_knowledge_high_gap() -> None:
    sc = SigmaConscious(gate=_OscGate())
    sc.predict_own_σ("x", "y")
    sc.predict_own_σ("x", "y")
    out = sc.predict_own_σ("x", "y")
    assert out["self_knowledge"] == "poor"


def test_perturbation_complexity() -> None:
    sc = SigmaConscious()
    g = _FlatGate()

    def pert(s: str) -> str:
        return s + " NOISE"

    pc = sc.perturbation_complexity(g, ["aa", "bb", "cc"], pert)
    assert "complexity" in pc and "n_tests" in pc
    assert pc["n_tests"] == 3


def test_phi_proxy_integrated_lower() -> None:
    class _WholeLower:
        def score(self, p: str, r: str):
            blob = p + r
            if len(blob) > 80:
                return 0.1, "ACCEPT"
            return 0.6, "RETHINK"

    sc = SigmaConscious()
    g = _WholeLower()
    out = sc.phi_proxy(g, [("p1", "a"), ("p2", "b")])
    assert out["phi_proxy"] >= 0.0
    assert "integrated" in out


def test_awareness_report() -> None:
    sc = SigmaConscious(gate=_FlatGate())
    sc.predict_own_σ("p", "r")
    sc.predict_own_σ("q", "s")
    rep = sc.awareness_report()
    assert "σ_meta" in rep and "calibration" in rep
    assert rep["self_knowledge_samples"] >= 2


def test_sigma_trend() -> None:
    sc = SigmaConscious(gate=_FlatGate())
    sc.σ_history.extend([0.8, 0.7, 0.55])
    assert sc._σ_trend(window=10) == "improving"

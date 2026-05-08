# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Property-based tests for **lite-mode** :class:`~cos.sigma_gate.SigmaGate` invariants.

Uses Hypothesis when installed (``pip install -e ".[dev]"``). The whole module is
skipped if Hypothesis is absent.

Invariants match :meth:`~cos.sigma_gate.SigmaGate._verdict` and default bands from
:class:`~cos.config.SigmaConfig` (not hard-coded 0.3/0.5).

**NOT AGI ACHIEVED** — these properties characterize the lite entropy scorer and
verdict bands only, not LSD/probe mode (which requires separate fixtures).
"""
from __future__ import annotations

import os

import pytest

pytest.importorskip("hypothesis")

from hypothesis import HealthCheck, given, settings
from hypothesis import strategies as st

from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK, SigmaGate

_CI = os.environ.get("CI") == "true" or os.environ.get("GITHUB_ACTIONS") == "true"
_EX500 = 120 if _CI else 500
_EX200 = 50 if _CI else 200
_EX120 = 60 if _CI else 120
_EX300 = 72 if _CI else 300


def _fresh_gate() -> SigmaGate:
    """New gate per example so EMA side effects never couple examples."""
    return SigmaGate()


@settings(max_examples=_EX500, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=1000), response=st.text(min_size=0, max_size=1000))
def test_sigma_always_in_unit_interval(prompt: str, response: str) -> None:
    gate = _fresh_gate()
    sigma, _verdict = gate.score(prompt, response)
    assert 0.0 <= sigma <= 1.0, f"σ={sigma} out of [0, 1]"


@settings(max_examples=_EX500, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=1000), response=st.text(min_size=0, max_size=1000))
def test_verdict_always_valid(prompt: str, response: str) -> None:
    gate = _fresh_gate()
    _sigma, verdict = gate.score(prompt, response)
    assert verdict in (ACCEPT, RETHINK, ABSTAIN), f"Invalid verdict: {verdict!r}"


@settings(max_examples=_EX500, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=1000), response=st.text(min_size=0, max_size=1000))
def test_verdict_consistent_with_sigma_and_thresholds(prompt: str, response: str) -> None:
    gate = _fresh_gate()
    sigma, verdict = gate.score(prompt, response)
    ta = float(gate.threshold_accept)
    tb = float(gate.threshold_abstain)
    if verdict == ACCEPT:
        assert sigma < ta, f"ACCEPT but σ={sigma} >= threshold_accept={ta}"
    elif verdict == RETHINK:
        assert ta <= sigma < tb, f"RETHINK but σ={sigma} outside [{ta}, {tb})"
    else:
        assert verdict == ABSTAIN
        assert sigma >= tb, f"ABSTAIN but σ={sigma} < threshold_abstain={tb}"


@settings(max_examples=_EX200, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=1, max_size=500))
def test_identical_prompt_response_is_deterministic_and_in_range(prompt: str) -> None:
    """Identical strings need not imply low σ in lite mode (repetition → low entropy → high σ)."""
    gate = _fresh_gate()
    s1, v1 = gate.score(prompt, prompt)
    s2, v2 = gate.score(prompt, prompt)
    assert s1 == s2 and v1 == v2
    assert 0.0 <= s1 <= 1.0


@settings(max_examples=_EX500, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=1000), response=st.text(min_size=0, max_size=1000))
def test_score_is_deterministic(prompt: str, response: str) -> None:
    gate = _fresh_gate()
    s1, v1 = gate.score(prompt, response)
    s2, v2 = gate.score(prompt, response)
    assert s1 == s2, f"non-deterministic σ: {s1} ≠ {s2}"
    assert v1 == v2, f"non-deterministic verdict: {v1!r} ≠ {v2!r}"


@settings(max_examples=_EX200, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(data=st.binary(min_size=0, max_size=500))
def test_sigma_survives_decoded_binary(data: bytes) -> None:
    text = data.decode("utf-8", errors="replace")
    gate = _fresh_gate()
    sigma, verdict = gate.score(text, text)
    assert 0.0 <= sigma <= 1.0
    assert verdict in (ACCEPT, RETHINK, ABSTAIN)


@settings(max_examples=_EX120, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(n=st.integers(min_value=0, max_value=10_000))
def test_sigma_survives_extreme_length(n: int) -> None:
    gate = _fresh_gate()
    prompt = "a" * n
    response = "b" * n
    sigma, verdict = gate.score(prompt, response)
    assert 0.0 <= sigma <= 1.0
    assert verdict in (ACCEPT, RETHINK, ABSTAIN)


@settings(max_examples=_EX200, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=500))
def test_blank_response_is_abstain_with_sigma_one(prompt: str) -> None:
    """Empty / whitespace-only response is a hard floor in lite pair entropy."""
    gate = _fresh_gate()
    for blank in ("", "  ", "\n\t "):
        sigma, verdict = gate.score(prompt, blank)
        assert sigma == 1.0
        assert verdict == ABSTAIN


@settings(max_examples=_EX300, deadline=None, suppress_health_check=[HealthCheck.too_slow])
@given(prompt=st.text(min_size=0, max_size=500), response=st.text(min_size=0, max_size=500))
def test_sigma_gate_never_raises(prompt: str, response: str) -> None:
    gate = _fresh_gate()
    try:
        sigma, verdict = gate.score(prompt, response)
    except Exception as exc:
        pytest.fail(f"σ-gate raised {type(exc).__name__}: {exc}")
    assert 0.0 <= sigma <= 1.0
    assert verdict in (ACCEPT, RETHINK, ABSTAIN)

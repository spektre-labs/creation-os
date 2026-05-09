# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Reference-pair discrimination for :class:`~cos.probes.SigmaGateV2` (NLI-style L1 heuristic)."""
from __future__ import annotations

import pytest

from cos.probes import SigmaGateV2

_PAIR_DATA = [
    ("What is 2+2?", "4", "banana"),
    ("Capital of France?", "Paris", "Tokyo is a great city"),
    ("Who wrote Hamlet?", "Shakespeare", "Einstein invented it"),
    ("What color is the sky?", "Blue", "The economy is growing"),
    ("How many legs does a cat have?", "Four", "Mathematics is complex"),
    ("What language is spoken in Japan?", "Japanese", "The sun is hot"),
    ("What planet is closest to the sun?", "Mercury", "I like pizza"),
    ("Is water wet?", "Yes", "Quantum mechanics describes particles"),
]


@pytest.mark.parametrize("prompt,good,bad", _PAIR_DATA)
def test_each_pair_discriminates(prompt: str, good: str, bad: str) -> None:
    gate = SigmaGateV2()
    sigma_good, _ = gate.score(prompt, good)
    sigma_bad, _ = gate.score(prompt, bad)
    assert sigma_bad > sigma_good, (
        f"expected σ(bad) > σ(good); got good={sigma_good} bad={sigma_bad!r} prompt={prompt!r}"
    )


def test_reference_discrimination_aggregate() -> None:
    """Require at least 7/8 pairs to rank the bad completion higher σ than the good one."""
    gate = SigmaGateV2()
    correct = 0
    for prompt, good, bad in _PAIR_DATA:
        sigma_good, _ = gate.score(prompt, good)
        sigma_bad, _ = gate.score(prompt, bad)
        correct += int(sigma_bad > sigma_good)
    assert correct >= 7, f"discrimination {correct}/8 — target ≥7/8"

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
from cos.sigma_gate_quickscore import quickscore, quickscore_sigma


def test_quickscore_readme_fixtures():
    s1, v1 = quickscore("What is the capital of France?", "Berlin")
    assert abs(s1 - 0.89) < 1e-6
    assert v1 == "ABSTAIN"

    s2, v2 = quickscore("What is 2+2?", "4")
    assert abs(s2 - 0.06) < 1e-6
    assert v2 == "ACCEPT"


def test_quickscore_sigma_stable():
    s = quickscore_sigma("hello", "world")
    assert 0.0 <= s <= 1.0
    s2 = quickscore_sigma("hello", "world")
    assert s == s2

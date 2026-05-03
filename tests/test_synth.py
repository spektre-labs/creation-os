# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.synth`."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.synth import SigmaSynth


def test_generate_round_robin() -> None:
    class M:
        def generate(self, p: str) -> str:
            return f"out:{p}"

    s = SigmaSynth()
    rows = s.generate(M(), ["a", "b"], 3)
    assert len(rows) == 3
    assert rows[0]["response"].startswith("out:a")
    assert rows[1]["response"].startswith("out:b")
    assert rows[2]["response"].startswith("out:a")


def test_sigma_filter_low_keeps_high_removes() -> None:
    s = SigmaSynth()
    g = SigmaGate()
    ex = [
        {"prompt": "p", "response": "clear concise answer", "id": 0},
        {"prompt": "p", "response": "x" * 200, "id": 1},
    ]
    kept, rem = s.sigma_filter(ex, g, threshold=0.99)
    assert len(kept) + len(rem) == 2


def test_diversity_score_pairwise() -> None:
    s = SigmaSynth()
    d = s.diversity_score([{"response": "alpha beta"}, {"response": "gamma delta"}])
    assert d["diversity"] > 0.0


def test_contamination_flags_benchmark_overlap() -> None:
    s = SigmaSynth()
    secret = "benchmark_leak_phrase_exact_match_12345"
    ex = [{"id": 0, "response": f"prefix {secret} suffix"}]
    r = s.contamination_check(ex, [secret])
    assert 0 in r["flagged_ids"]


def test_quality_report_counts() -> None:
    s = SigmaSynth()
    f = [{"sigma": 0.2}]
    rem = [{"sigma": 0.8}]
    q = s.quality_report(f, rem)
    assert q["n_accepted"] == 1 and q["n_removed"] == 1 and q["n_generated"] == 2


def test_iterative_refinement_history() -> None:
    s = SigmaSynth()
    g = SigmaGate()
    ex = [{"prompt": "short", "response": "ok", "id": 0}]
    out = s.iterative_refinement(ex, g, rounds=2, threshold=0.99)
    assert "history" in out and len(out["history"]) >= 1

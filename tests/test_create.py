# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.create import SigmaCreate


class _ConstGate:
    def __init__(self, σ: float = 0.2) -> None:
        self.σ = float(σ)

    def score(self, prompt: str, response: str):
        del prompt, response
        return self.σ, "ACCEPT"


class _EntityGraph:
    def __init__(self, entities: list) -> None:
        self._e = list(entities)

    def entities(self):
        return list(self._e)


def test_generate_novel_scores_candidates() -> None:
    g = _EntityGraph(["cat", "dog"])
    c = SigmaCreate(gate=_ConstGate(0.15), graph=g)
    out = c.generate_novel("animals", ["cat runs", "zebra flies"])
    assert len(out) == 2
    assert all("creativity" in row and "boden_type" in row for row in out)


def test_creativity_is_novelty_times_quality() -> None:
    g = _EntityGraph(["cat"])
    c = SigmaCreate(gate=_ConstGate(0.4), graph=g)
    row = c.generate_novel("x", ["only zebras here"])[0]
    q = row["quality"]
    n = row["novelty"]
    assert abs(row["creativity"] - q * n) < 1e-6


def test_high_novelty_high_quality_wins() -> None:
    class _PickGate:
        def score(self, p: str, r: str):
            del p
            return (0.05, "ACCEPT") if "WINNER" in r.upper() else (0.7, "RETHINK")

    c = SigmaCreate(gate=_PickGate(), graph=_EntityGraph([]))
    out = c.generate_novel("task", ["WINNER proposal", "other proposal"])
    assert out[0]["candidate"] == "WINNER proposal"


def test_elegance_short_correct_high() -> None:
    c = SigmaCreate(gate=_ConstGate(0.1))
    assert c.elegance_score("yes") > c.elegance_score("yes " + "word " * 20)


def test_elegance_long_correct_lower() -> None:
    c = SigmaCreate(gate=_ConstGate(0.1))
    short = c.elegance_score("two words")
    long = c.elegance_score("word " * 30)
    assert short > long


def test_boden_combinatorial() -> None:
    c = SigmaCreate(graph=_EntityGraph(["red", "blue", "green", "yellow"]))
    # Mostly tokens overlap known entities → low novelty → combinatorial
    row = c.generate_novel("x", ["red blue green yellow mix"])[0]
    assert row["boden_type"] == "combinatorial"


def test_boden_transformational() -> None:
    c = SigmaCreate(graph=_EntityGraph(["red"]))
    row = c.generate_novel("x", ["quantum flux capacitor idea"])[0]
    assert row["boden_type"] == "transformational"


def test_divergent_think_produces_directions() -> None:
    c = SigmaCreate()
    dirs = c.divergent_think("solve traffic", n_directions=3)
    assert len(dirs) == 3
    assert all("direction" in d and "σ" in d for d in dirs)

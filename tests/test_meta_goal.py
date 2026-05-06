# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.meta_goal import SigmaMetaGoal


def test_register_skill() -> None:
    mg = SigmaMetaGoal()
    mg.register_skill("parse")
    assert "parse" in mg.skills
    assert mg.skills["parse"] == []


def test_record_sigma() -> None:
    mg = SigmaMetaGoal()
    mg.record("parse", 0.9)
    mg.record("parse", 0.85)
    assert mg.skills["parse"] == [0.9, 0.85]


def test_learning_progress_positive() -> None:
    mg = SigmaMetaGoal()
    for s in (1.0, 0.92, 0.84, 0.76, 0.68):
        mg.record("skill_a", s)
    assert mg.learning_progress("skill_a", window=5) > 0.0


def test_learning_progress_zero_when_stuck() -> None:
    mg = SigmaMetaGoal()
    for _ in range(6):
        mg.record("flat", 0.55)
    assert mg.learning_progress("flat", window=5) == 0.0


def test_generate_goals_ranks_by_interest() -> None:
    mg = SigmaMetaGoal()
    for s in (1.0, 0.95, 0.9, 0.85, 0.8):
        mg.record("fast", s)
    for _ in range(5):
        mg.record("slow", 0.7)
    ranked = mg.generate_goals(top_n=10)
    names = [g["skill"] for g in ranked]
    assert names.index("fast") < names.index("slow")


def test_mastered_skill_boring() -> None:
    mg = SigmaMetaGoal()
    for s in (0.2, 0.12, 0.08, 0.07, 0.06):
        mg.record("done", s)
    goals = mg.generate_goals(top_n=20)
    row = next(g for g in goals if g["skill"] == "done")
    assert row["interest"] == 0.0


def test_impossible_skill_skipped() -> None:
    mg = SigmaMetaGoal()
    for _ in range(5):
        mg.record("blocked", 0.99)
    goals = mg.generate_goals(top_n=20)
    row = next(g for g in goals if g["skill"] == "blocked")
    assert row["interest"] == -1.0
    assert row["learning_progress"] == 0.0


def test_curriculum_excludes_mastered() -> None:
    mg = SigmaMetaGoal()
    for s in (0.15, 0.09, 0.08, 0.07):
        mg.record("mastered", s)
    for s in (1.0, 0.9, 0.82):
        mg.record("active", s)
    cur = mg.build_curriculum()
    skills = {c["skill"] for c in cur}
    assert "mastered" not in skills
    assert "active" in skills

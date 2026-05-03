# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.continual import SigmaContinual


def test_learn() -> None:
    cl = SigmaContinual()
    result = cl.learn([{"prompt": "test", "response": "hello"}])
    assert result["learned"]
    assert result["lora_count"] >= 1


def test_register_skill() -> None:
    cl = SigmaContinual()
    cl.register_skill("math", "2+2?", "4")
    assert "math" in cl.skill_baselines


def test_check_regression() -> None:
    cl = SigmaContinual()
    cl.register_skill("math", "2+2?", "4")
    result = cl.check_regression()
    assert "regressed" in result
    assert result["skills_checked"] == 1


def test_multiple_learns() -> None:
    cl = SigmaContinual()
    cl.learn([{"prompt": "a", "response": "b"}])
    cl.learn([{"prompt": "c", "response": "d"}])
    assert cl.stats()["lora_count"] == 2


def test_stats() -> None:
    cl = SigmaContinual()
    stats = cl.stats()
    assert stats["lora_count"] == 0
    assert stats["merged"] is False

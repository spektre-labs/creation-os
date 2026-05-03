# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.compose`."""
from __future__ import annotations

import json
from pathlib import Path

from cos.compose import SigmaCompose


def test_pipeline_propagates_sigma() -> None:
    c = SigmaCompose()
    step1 = lambda ctx: {**ctx, "sigma": 0.2}
    step2 = lambda ctx: {**ctx, "sigma": max(float(ctx["sigma"]), 0.4)}
    pipe = c.pipeline(step1, step2)
    out = pipe({"prompt": "x"})
    assert out["_sigma_max"] >= 0.4


def test_sigma_propagation_helper() -> None:
    ctx = {"_sigmas": [0.1, 0.7, 0.2]}
    assert SigmaCompose.sigma_propagation(ctx) == 0.7


def test_branch() -> None:
    c = SigmaCompose()
    pipe = c.branch(lambda x: x.get("mode") == "fast", lambda y: {**y, "branch": "a"}, lambda y: {**y, "branch": "b"})
    assert pipe({"mode": "fast"})["branch"] == "a"
    assert pipe({"mode": "slow"})["branch"] == "b"


def test_parallel_min_sigma() -> None:
    c = SigmaCompose()
    a = lambda x: {**x, "sigma": 0.9}
    b = lambda x: {**x, "sigma": 0.1}
    pipe = c.parallel(a, b)
    out = pipe({})
    assert out["sigma"] == 0.1


def test_retry_on_rethink() -> None:
    c = SigmaCompose()
    n = {"i": 0}

    def step(ctx):
        n["i"] += 1
        return {**ctx, "verdict": "ACCEPT" if n["i"] >= 2 else "RETHINK", "sigma": 0.5}

    pipe = c.retry(step, max_retries=3, on="RETHINK")
    out = pipe({})
    assert out["verdict"] == "ACCEPT"


def test_circuit_breaker_opens() -> None:
    c = SigmaCompose()

    def bad(ctx):
        return {**ctx, "verdict": "ABSTAIN", "sigma": 1.0}

    pipe = c.circuit_breaker(bad, max_fails=2)
    pipe({})
    pipe({})
    out = pipe({})
    assert out.get("circuit_open") is True


def test_from_yaml_json(tmp_path: Path) -> None:
    c = SigmaCompose()
    p = tmp_path / "pipe.yaml"
    p.write_text(json.dumps({"steps": ["retrieve", "gate"]}), encoding="utf-8")
    pipe = c.from_yaml(p)
    out = pipe({"sigma": 0.0})
    assert out.get("verdict") == "ACCEPT"

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import ast
import json
import os
import subprocess
import sys
from pathlib import Path

from cos import SigmaGate
from cos.integrations.decorator import SigmaResult, sigma_gated


def test_basic_score_accept() -> None:
    gate = SigmaGate()
    sigma, verdict = gate.score("What is the capital of France?", "Yes.")
    assert verdict == "ACCEPT"
    assert float(sigma) < float(gate.threshold_accept)


def test_basic_score_rethink() -> None:
    gate = SigmaGate()
    r = " ".join(["red"] * 6)
    sigma, verdict = gate.score("Name any color.", r)
    assert verdict == "RETHINK"
    assert float(gate.threshold_accept) <= float(sigma) < float(gate.threshold_abstain)


def test_decorator_works() -> None:
    @sigma_gated
    def fake_llm(prompt: str) -> str:
        return f"echo:{prompt[:8]}"

    out = fake_llm("hello world")
    assert isinstance(out, SigmaResult)
    assert hasattr(out, "sigma") and hasattr(out, "verdict") and hasattr(out, "text")


def test_cli_score_returns_sigma() -> None:
    root = Path(__file__).resolve().parents[1]
    env = {**os.environ, "PYTHONPATH": str(root / "python")}
    proc = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "score",
            "--prompt",
            "What is 2+2?",
            "--response",
            "4",
            "--json",
        ],
        cwd=str(root),
        env=env,
        capture_output=True,
        text=True,
        timeout=60,
        check=False,
    )
    assert proc.returncode in (0, 2), proc.stderr
    payload = json.loads(proc.stdout.strip())
    assert "sigma" in payload and "verdict" in payload


def test_examples_importable() -> None:
    root = Path(__file__).resolve().parents[1]
    ex_dir = root / "examples"
    for path in sorted(ex_dir.glob("*.py")):
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))

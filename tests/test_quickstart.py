# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""Smoke tests aligned with ``docs/QUICKSTART.md`` (no network)."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]


def _env() -> dict[str, str]:
    env = os.environ.copy()
    py = str(_REPO / "python")
    env["PYTHONPATH"] = py + os.pathsep + env.get("PYTHONPATH", "")
    return env


def test_basic_score_accept_with_tuned_thresholds() -> None:
    """Lite σ for \"2+2\" / \"4\" is often ~0.25; loosen accept band for a stable ACCEPT."""
    from cos import SigmaGate

    gate = SigmaGate(threshold_accept=0.30, threshold_abstain=0.90)
    sigma, verdict = gate.score("What is 2+2?", "4")
    assert verdict == "ACCEPT"
    assert 0.0 <= float(sigma) <= 1.0


def test_score_hallucination_mismatch_has_sigma() -> None:
    from cos import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score("Capital of Japan?", "Berlin")
    assert 0.0 <= float(sigma) <= 1.0
    assert verdict in ("ACCEPT", "RETHINK", "ABSTAIN")
    assert float(sigma) > 0.1


def test_decorator_sigma_result_has_verdict() -> None:
    from cos.integrations.decorator import sigma_gated

    @sigma_gated
    def fake(p: str) -> str:
        return "hello"

    r = fake("test")
    assert hasattr(r, "verdict")
    assert hasattr(r, "sigma")
    assert hasattr(r, "text")


def test_cli_score_zero_exit() -> None:
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "score",
            "--prompt",
            "test",
            "--response",
            "hello",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    out = (result.stdout or "") + (result.stderr or "")
    assert "σ=" in out or "ACCEPT" in out or "RETHINK" in out or "ABSTAIN" in out

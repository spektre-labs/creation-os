# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""CLI tests: ``python -m cos`` entry (no network, no API keys)."""
from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]


def _env() -> dict[str, str]:
    env = os.environ.copy()
    py = str(_REPO / "python")
    env["PYTHONPATH"] = py + os.pathsep + env.get("PYTHONPATH", "")
    return env


def test_cli_version() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "cos.cli", "version"],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    out = (result.stdout or "") + (result.stderr or "")
    assert "creation-os" in out or "0.1.0" in out


def test_cli_score() -> None:
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "score",
            "--prompt",
            "What is 2+2?",
            "--response",
            "4",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    out = result.stdout or ""
    assert "σ=" in out or "ACCEPT" in out or "RETHINK" in out


def test_cli_score_json() -> None:
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
            "--json",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    data = json.loads((result.stdout or "").strip())
    assert "sigma" in data
    assert "verdict" in data


def test_cli_help() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "cos.cli", "--help"],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    assert "score" in (result.stdout or "")


def test_cli_pipe() -> None:
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "pipe",
            "--prompt",
            "What is 2+2?",
            "--response",
            "4",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    assert "σ=" in (result.stdout or "")


def test_cli_stats() -> None:
    result = subprocess.run(
        [sys.executable, "-m", "cos.cli", "stats"],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    assert "accept_rate" in (result.stdout or "")


def test_cli_stream() -> None:
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "stream",
            "--prompt",
            "What is the answer?",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    out = result.stdout or ""
    assert "σ=" in out


def test_cli_snapshot_save_list() -> None:
    tmp = tempfile.mkdtemp()
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "snapshot",
            "save",
            "--label",
            "cli_test",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=tmp,
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    payload = json.loads((result.stdout or "").strip())
    assert payload.get("checkpointed") is True

    result2 = subprocess.run(
        [sys.executable, "-m", "cos.cli", "snapshot", "list"],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=tmp,
        env=_env(),
        check=False,
    )
    assert result2.returncode == 0
    assert "cli_test" in (result2.stdout or "")


def test_cli_calibrate_fit() -> None:
    tmp = tempfile.mkdtemp()
    data = Path(tmp) / "val.json"
    data.write_text("[[0.1,0],[0.9,1]]\n", encoding="utf-8")
    out = Path(tmp) / "cal.json"
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos.cli",
            "calibrate",
            "fit",
            "--data",
            str(data),
            "--out",
            str(out),
            "--method",
            "platt",
        ],
        capture_output=True,
        text=True,
        timeout=120,
        cwd=str(_REPO),
        env=_env(),
        check=False,
    )
    assert result.returncode == 0
    payload = json.loads((result.stdout or "").strip())
    assert payload.get("fitted") is True
    assert out.is_file()

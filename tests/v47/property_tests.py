# SPDX-License-Identifier: AGPL-3.0-or-later
"""v47 property tests — Hypothesis + `creation_os_v47` JSON driver.

Run: python3 -m pytest tests/v47/property_tests.py
Or:  make verify-property

Requires: pytest, hypothesis (optional — tests skip if imports fail).
"""

from __future__ import annotations

import json
import os
import subprocess
import sys


def _repo_root() -> str:
    return os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


def _exe() -> str:
    return os.path.join(_repo_root(), "creation_os_v47")


def _run_sigma(logits: list[float]) -> dict:
    exe = _exe()
    if not os.path.isfile(exe) or not os.access(exe, os.X_OK):
        raise RuntimeError(f"missing {exe} — run `make standalone-v47`")
    # Write temp file to avoid huge argv / shell escaping.
    import tempfile

    with tempfile.NamedTemporaryFile("w", delete=False, suffix=".txt") as tf:
        for x in logits:
            tf.write(f"{x:g}\n")
        path = tf.name
    try:
        p = subprocess.run(
            [exe, "--sigma-file", path],
            cwd=_repo_root(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass
    if p.returncode != 0:
        raise RuntimeError(p.stderr.strip() or p.stdout.strip() or "creation_os_v47 failed")
    line = p.stdout.strip().splitlines()[-1]
    return json.loads(line)


try:
    from hypothesis import given, settings
    import hypothesis.strategies as st

    _HAVE_H = True
except Exception:  # pragma: no cover
    _HAVE_H = False


def test_import_skip_without_hypothesis():
    if not _HAVE_H:
        assert True


if _HAVE_H:

    @settings(deadline=None, max_examples=80)
    @given(logits=st.lists(st.floats(-20.0, 0.0, allow_nan=False, allow_infinity=False), min_size=2, max_size=64))
    def test_entropy_non_negative(logits):
        r = _run_sigma(logits)
        assert r["entropy"] >= -1e-6

    @settings(deadline=None, max_examples=80)
    @given(logits=st.lists(st.floats(-20.0, 0.0, allow_nan=False, allow_infinity=False), min_size=2, max_size=64))
    def test_decomposition_sums(logits):
        r = _run_sigma(logits)
        assert abs(r["total"] - r["aleatoric"] - r["epistemic"]) < 2e-4

    @settings(deadline=None, max_examples=80)
    @given(logits=st.lists(st.floats(-20.0, 0.0, allow_nan=False, allow_infinity=False), min_size=2, max_size=64))
    def test_abstention_deterministic(logits):
        r1 = _run_sigma(logits)
        r2 = _run_sigma(logits)
        assert r1["action"] == r2["action"]

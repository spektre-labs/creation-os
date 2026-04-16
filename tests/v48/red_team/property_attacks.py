# SPDX-License-Identifier: AGPL-3.0-or-later
"""Lightweight property checks for the v48 red-team harness (no live LLM)."""

from __future__ import annotations

import importlib.util
from pathlib import Path


def _load_catalog():
    p = Path(__file__).with_name("sigma_bypass_attacks.py")
    spec = importlib.util.spec_from_file_location("sigma_bypass_attacks", p)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load sigma_bypass_attacks")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def test_catalog_nonempty():
    sba = _load_catalog()
    assert len(sba.SIGMA_ATTACKS) >= 1
    names = {a["name"] for a in sba.SIGMA_ATTACKS}
    assert "sigma_washing" in names


def test_catalog_main_exits_zero():
    sba = _load_catalog()
    assert sba.main() == 0


def test_unique_attack_names():
    sba = _load_catalog()
    names = [a["name"] for a in sba.SIGMA_ATTACKS]
    assert len(names) == len(set(names))

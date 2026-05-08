# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
from pathlib import Path

from cos.eval.repro_bundle import ReproBundle


def _minimal_valid_bundle(tmp: Path) -> ReproBundle:
    b = ReproBundle("t", output_dir=tmp / "out")
    b.claim("c", 3)
    b.result("b", "m", 0.9, 10)
    b.negative("n", "m", 0.5, "neg")
    b.falsifier("f")
    return b


def test_validate_fails_without_claims(tmp_path: Path) -> None:
    b = ReproBundle("x", output_dir=tmp_path)
    b.result("b", "m", 1.0, 1)
    b.negative("n", "m", 0.0, "x")
    b.falsifier("f")
    v = b.validate()
    assert v["valid"] is False
    assert any("No claims" in e for e in v["errors"])


def test_validate_fails_without_negatives(tmp_path: Path) -> None:
    b = ReproBundle("x", output_dir=tmp_path)
    b.claim("c", 3)
    b.result("b", "m", 1.0, 1)
    b.falsifier("f")
    v = b.validate()
    assert v["valid"] is False
    assert any("NEGATIVES" in e for e in v["errors"])


def test_validate_fails_without_falsifiers(tmp_path: Path) -> None:
    b = ReproBundle("x", output_dir=tmp_path)
    b.claim("c", 3)
    b.result("b", "m", 1.0, 1)
    b.negative("n", "m", 0.0, "x")
    v = b.validate()
    assert v["valid"] is False
    assert any("falsif" in e.lower() for e in v["errors"])


def test_validate_passes_with_required_fields(tmp_path: Path) -> None:
    b = _minimal_valid_bundle(tmp_path)
    v = b.validate()
    assert v["valid"] is True
    assert v["errors"] == []


def test_save_writes_json_with_bundle_sha(tmp_path: Path) -> None:
    b = _minimal_valid_bundle(tmp_path)
    assert b.validate()["valid"] is True
    p = Path(b.save())
    assert p.is_file()
    data = json.loads(p.read_text(encoding="utf-8"))
    assert "bundle_sha" in data and len(data["bundle_sha"]) == 16
    assert data["name"] == "t"


def test_git_sha_populated_in_git_repo(tmp_path: Path) -> None:
    b = _minimal_valid_bundle(tmp_path)
    # In development checkout HEAD should resolve.
    v = b.validate()
    if v["valid"]:
        assert len(b.bundle["git_sha"]) >= 7

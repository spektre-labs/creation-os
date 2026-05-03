# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.migrate`."""
from __future__ import annotations

import json
from pathlib import Path

from cos.migrate import SigmaMigrate


def test_migrate_check_version() -> None:
    m = SigmaMigrate()
    r = m.check_version("0.1.0", "0.2.0")
    assert r["needs_upgrade"] is True
    r2 = m.check_version("1.0.0", "0.9.0")
    assert r2["needs_upgrade"] is False


def test_migrate_config_roundtrip_fields() -> None:
    m = SigmaMigrate()
    out = m.migrate_config({"foo": 1}, "0.2.0")
    assert out["migrated_to"] == "0.2.0"
    assert out["foo"] == 1


def test_migrate_backup_rollback(tmp_path: Path) -> None:
    m = SigmaMigrate()
    cfg = tmp_path / "cfg"
    cfg.mkdir()
    (cfg / "a.json").write_text("{}", encoding="utf-8")
    bak = m.backup_before_migrate(cfg, tmp_path)
    (cfg / "a.json").write_text('{"x":1}', encoding="utf-8")
    rb = m.rollback(bak, cfg)
    assert rb["ok"] is True
    assert json.loads((cfg / "a.json").read_text()) == {}


def test_migrate_breaking_changes_major() -> None:
    m = SigmaMigrate()
    b = m.breaking_changes("0.9.0", "1.0.0")
    assert any("MAJOR" in x for x in b)

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Version and config migration lab — semver check, backup, rollback (filesystem, no network).

Pairs with registry / watchdog / offline bundle flows as **policy hooks**, not auto-updaters.
"""
from __future__ import annotations

import re
import shutil
import time
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Tuple

__all__ = ["SigmaMigrate"]


def _parse_semver(s: str) -> Tuple[int, int, int]:
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)", str(s).strip())
    if not m:
        raise ValueError(f"not semver: {s!r}")
    return int(m.group(1)), int(m.group(2)), int(m.group(3))


class SigmaMigrate:
    """Compare versions, transform configs, backup dirs, rollback from tar/dir copies."""

    def check_version(self, current: str, latest: str) -> Dict[str, Any]:
        a, b = _parse_semver(current), _parse_semver(latest)
        need = a < b
        return {
            "current": current,
            "latest": latest,
            "needs_upgrade": need,
            "tuple": {"current": a, "latest": b},
        }

    def breaking_changes(self, from_version: str, to_version: str) -> List[str]:
        """Static ledger; extend in-repo when APIs change."""
        fv, tv = _parse_semver(from_version), _parse_semver(to_version)
        out: List[str] = []
        if tv[0] > fv[0]:
            out.append("MAJOR bump: review CHANGELOG and run merge-gate before deploying.")
        if tv[1] > fv[1] and tv[0] == fv[0]:
            out.append("MINOR bump: new optional modules may appear; defaults should remain compatible.")
        if not out:
            out.append("PATCH or same-line bump: expect backward-compatible fixes only (verify notes).")
        return out

    def migrate_config(self, old_config: Mapping[str, Any], new_version: str) -> Dict[str, Any]:
        cfg = dict(old_config)
        cfg["migrated_to"] = str(new_version)
        cfg.setdefault("sigma_gate", {})
        if isinstance(cfg["sigma_gate"], dict):
            cfg["sigma_gate"].setdefault("lab_note", "migrated by SigmaMigrate")
        return cfg

    def migrate_probes(self, old_probes_dir: Path, new_version: str) -> Dict[str, Any]:
        root = Path(old_probes_dir)
        manifest = {
            "version": str(new_version),
            "probed_files": [],
            "migrated_at": time.time(),
        }
        if root.is_dir():
            for p in sorted(root.glob("*")):
                if p.is_file():
                    manifest["probed_files"].append(p.name)
        return manifest

    def backup_before_migrate(self, config_dir: Path, backup_parent: Optional[Path] = None) -> Path:
        src = Path(config_dir)
        parent = Path(backup_parent) if backup_parent else src.parent
        stamp = time.strftime("%Y%m%d_%H%M%S")
        dest = parent / f"{src.name}.backup_{stamp}"
        if src.is_dir():
            shutil.copytree(src, dest)
        else:
            parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest)
        return dest

    def rollback(self, backup_path: Path, target_path: Path) -> Dict[str, Any]:
        bp = Path(backup_path)
        tp = Path(target_path)
        if not bp.exists():
            return {"ok": False, "error": "backup missing"}
        if tp.exists():
            if tp.is_dir():
                shutil.rmtree(tp)
            else:
                tp.unlink()
        if bp.is_dir():
            shutil.copytree(bp, tp)
        else:
            tp.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(bp, tp)
        return {"ok": True, "restored": str(tp)}

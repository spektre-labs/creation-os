#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Archive exact duplicates between `gda 2/` and `gda/`.

Goal: if two files are byte-identical, move the duplicate into `gda/_archive/`
and leave the canonical copy in `gda/`.

This is intentionally conservative:
- only moves exact-byte matches
- never overwrites existing archived files
- leaves non-identical files untouched (prints a note)
"""

from __future__ import annotations

import hashlib
import os
import shutil
import sys
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    src_dup = root / "gda 2"
    canonical = root / "gda"
    archive = canonical / "_archive"

    if not src_dup.exists():
        print("archive_gda_duplicates: no `gda 2/` directory found; nothing to do.")
        return 0
    if not canonical.exists():
        print("archive_gda_duplicates: missing `gda/` canonical directory; abort.", file=sys.stderr)
        return 2

    archive.mkdir(parents=True, exist_ok=True)

    moved = 0
    kept = 0
    skipped = 0

    for p in sorted(src_dup.glob("**/*")):
        if p.is_dir():
            continue
        rel = p.relative_to(src_dup)
        q = canonical / rel
        if not q.exists() or not q.is_file():
            print(f"KEEP (no canonical match): {p}")
            kept += 1
            continue
        hp = sha256(p)
        hq = sha256(q)
        if hp != hq:
            print(f"SKIP (differs): {p} != {q}")
            skipped += 1
            continue

        dest = archive / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        if dest.exists():
            # avoid overwriting; make filename unique
            dest = dest.with_name(dest.name + f".dup.{hp[:12]}")
        print(f"MOVE duplicate: {p} -> {dest}")
        shutil.move(str(p), str(dest))
        moved += 1

    # Try to remove empty dirs under gda 2
    for d in sorted([x for x in src_dup.glob("**/*") if x.is_dir()], reverse=True):
        try:
            d.rmdir()
        except OSError:
            pass
    try:
        src_dup.rmdir()
    except OSError:
        pass

    print(f"archive_gda_duplicates: moved={moved} kept={kept} skipped={skipped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


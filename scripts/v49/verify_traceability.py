#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Lightweight traceability checks driven by docs/v49/certification/trace_manifest.json."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    manifest = root / "docs" / "v49" / "certification" / "trace_manifest.json"
    if not manifest.is_file():
        print(f"ERROR: missing {manifest}", file=sys.stderr)
        return 2

    data = json.loads(manifest.read_text(encoding="utf-8"))
    entries = data.get("entries", [])
    if not entries:
        print("ERROR: empty trace_manifest.json", file=sys.stderr)
        return 2

    failures = 0
    for e in entries:
        hlr = e.get("hlr", "?")
        rel = e.get("path", "")
        path = root / rel
        if not path.is_file():
            print(f"FAIL {hlr}: missing file {rel}")
            failures += 1
            continue

        text = path.read_text(encoding="utf-8", errors="replace")

        must = e.get("must_contain")
        if isinstance(must, str) and must and must not in text:
            print(f"FAIL {hlr}: expected substring not found in {rel}: {must!r}")
            failures += 1
            continue

        must_not = e.get("must_not_contain")
        if isinstance(must_not, str) and must_not and must_not in text:
            print(f"FAIL {hlr}: forbidden substring found in {rel}: {must_not!r}")
            failures += 1
            continue

        sym = e.get("symbol")
        if isinstance(sym, str) and sym and sym not in text:
            print(f"FAIL {hlr}: symbol {sym!r} not found in {rel}")
            failures += 1
            continue

        print(f"OK   {hlr}: {rel}")

    if failures:
        print(f"certify-trace: FAIL ({failures} issues)", file=sys.stderr)
        return 1
    print("certify-trace: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

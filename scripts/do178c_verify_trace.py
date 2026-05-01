#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Validate docs/do178c/trace.csv for DO-178C-oriented traceability scaffolding."""
from __future__ import annotations

import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CSV_PATH = os.path.join(ROOT, "docs", "do178c", "trace.csv")


def main() -> int:
    with open(CSV_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        print("do178c_verify_trace: no rows in trace.csv", file=sys.stderr)
        return 2

    missing = []
    bad_paths = []
    for r in rows:
        if not all((v or "").strip() for v in r.values()):
            missing.append(r.get("requirement_id", "?"))
            continue
        for key in ("design_file", "code_file"):
            p = (r.get(key) or "").strip()
            if not os.path.isfile(os.path.join(ROOT, p)):
                bad_paths.append((r.get("requirement_id", "?"), key, p))
        pe = (r.get("proof_or_formal_evidence") or "").strip()
        if pe.lower() != "n/a" and not os.path.isfile(os.path.join(ROOT, pe)):
            bad_paths.append((r.get("requirement_id", "?"), "proof_or_formal_evidence", pe))

    print(
        f"do178c_verify_trace: {len(rows)} requirements, "
        f"{len(missing)} incomplete rows, {len(bad_paths)} bad paths"
    )
    if missing:
        print("  incomplete requirement_id:", ", ".join(missing), file=sys.stderr)
    for rid, k, p in bad_paths:
        print(f"  missing file [{rid}] {k} -> {p}", file=sys.stderr)
    if missing or bad_paths:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

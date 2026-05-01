#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
v156 σ-Paper — renders + structure validator.

Asserts the single unifying paper at
    docs/papers/creation_os_v1.md
contains all 11 sections named in the commit-queue spec, has a
minimum word count floor (> 1200 words), and a small set of
required references that the rest of the merge-gate already
guarantees exist (repo URL, CITATION.cff-style bibtex key,
measurement back-link to benchmarks/v111/).

SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT  = Path(__file__).resolve().parent.parent
PAPER = ROOT / "docs" / "papers" / "creation_os_v1.md"
WORD_FLOOR = 1200

REQUIRED_SECTIONS = [
    ("abstract",        r"^##\s+Abstract"),
    ("intro",           r"^##\s+1\.\s+Introduction"),
    ("sigma_theory",    r"^##\s+2\.\s+σ-theory"),
    ("architecture",    r"^##\s+3\.\s+Architecture"),
    ("measurements",    r"^##\s+4\.\s+Measurements"),
    ("comparisons",     r"^##\s+5\.\s+Comparisons"),
    ("living_system",   r"^##\s+6\.\s+Living system"),
    ("formal",          r"^##\s+7\.\s+Formal guarantees"),
    ("limitations",     r"^##\s+8\.\s+Limitations"),
    ("future",          r"^##\s+9\.\s+Future work"),
    ("repro",           r"^##\s+10\.\s+Reproducibility"),
    ("ack",             r"^##\s+11\.\s+Acknowledgments"),
]

REQUIRED_REFS = [
    "github.com/spektre-labs/creation-os",
    "benchmarks/v111",
    "@misc{creation_os_v1",
    "CC-BY-4.0",
    "SCSL-1.0",
    "BitNet-b1.58",
]


def fail(code: str, msg: str, errors: list[str]) -> None:
    errors.append(f"  [{code}] {msg}")


def main() -> int:
    errors: list[str] = []
    print("v156 paper-check: validating docs/papers/creation_os_v1.md")
    if not PAPER.exists():
        print(f"  [R0] missing: {PAPER.relative_to(ROOT)}")
        return 1
    text  = PAPER.read_text(encoding="utf-8")
    words = len(re.findall(r"\b[\w-]+\b", text))
    if words < WORD_FLOOR:
        fail("R1", f"word count {words} < floor {WORD_FLOOR}", errors)

    for sid, pattern in REQUIRED_SECTIONS:
        if not re.search(pattern, text, re.MULTILINE):
            fail("R2", f"section {sid!r} missing (regex={pattern!r})", errors)

    for ref in REQUIRED_REFS:
        if ref not in text:
            fail("R3", f"required reference absent: {ref!r}", errors)

    h2s = re.findall(r"^##\s+(.+)$", text, re.MULTILINE)
    if len(h2s) < 12:
        fail("R4", f"found {len(h2s)} ##-headings (want >= 12: Abstract + 11 sections)", errors)

    if not re.search(r"\*\*σ_product\*\*|σ_product", text):
        fail("R5", "paper must mention σ_product", errors)

    if "v152" not in text or "v154" not in text or "v153" not in text:
        fail("R6", "paper must reference v152, v153, and v154 explicitly", errors)

    if errors:
        print("v156 paper-check: FAIL")
        for e in errors:
            print(e)
        return 1
    print(f"  sections: {len(REQUIRED_SECTIONS)}/{len(REQUIRED_SECTIONS)} present")
    print(f"  words:    {words} (floor {WORD_FLOOR})")
    print(f"  refs:     {len(REQUIRED_REFS)}/{len(REQUIRED_REFS)} found")
    print("v156 paper-check: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())

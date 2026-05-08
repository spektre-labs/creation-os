# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Entry for ``python -m cos`` and the ``cos`` console script (see ``pyproject.toml``).

Runs minimal argv handlers for ``score`` and ``version`` before importing
:mod:`cos.cli` so common paths avoid loading the full argparse / lab surface.
"""
from __future__ import annotations

import json
import sys
from typing import Optional, Tuple


def _parse_score_fast(argv: list[str]) -> Optional[Tuple[str, str, bool]]:
    if not argv or argv[0] != "score":
        return None
    prompt: Optional[str] = None
    response: Optional[str] = None
    want_json = False
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in ("-h", "--help", "-v", "--verbose"):
            return None
        if a == "--json":
            want_json = True
            i += 1
            continue
        if a == "--prompt" and i + 1 < len(argv):
            prompt = argv[i + 1]
            i += 2
            continue
        if a == "--response" and i + 1 < len(argv):
            response = argv[i + 1]
            i += 2
            continue
        return None
    if prompt is None or response is None:
        return None
    return prompt, response, want_json


def _run_score_fast(prompt: str, response: str, want_json: bool) -> int:
    from cos.sigma_gate import SigmaGate

    gate = SigmaGate()
    sigma, verdict = gate.score(prompt, response)
    if want_json:
        print(json.dumps({"sigma": float(sigma), "verdict": str(verdict)}, ensure_ascii=False))
    else:
        print(f"σ={sigma:.4f} {verdict}")
    return 2 if verdict == "ABSTAIN" else 0


def _parse_version_fast(argv: list[str]) -> Optional[bool]:
    if not argv or argv[0] != "version":
        return None
    want_json = False
    for a in argv[1:]:
        if a == "--json":
            want_json = True
        elif a in ("-v", "--verbose"):
            continue
        else:
            return None
    return want_json


def _run_version_fast(want_json: bool) -> int:
    from cos import __version__

    if want_json:
        print(json.dumps({"package": "creation-os", "version": __version__}, ensure_ascii=False))
    else:
        print(f"creation-os {__version__}")
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    p = _parse_score_fast(argv)
    if p is not None:
        pr, rs, j = p
        return _run_score_fast(pr, rs, j)
    v = _parse_version_fast(argv)
    if v is not None:
        return _run_version_fast(v)

    from cos.cli import main as cli_main

    return cli_main(argv)


if __name__ == "__main__":
    raise SystemExit(main())

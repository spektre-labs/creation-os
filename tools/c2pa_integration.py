#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""
Optional bridge from Creation OS σ sidecars to C2PA tooling.

The in-repo contract is the JSON sidecar from `cos-stamp` / `cos chat --stamp`
(assertion label `ai.creation-os.sigma`).  Embed it with the C2PA SDK version
you use in production (APIs differ); this file only loads JSON and can print
the payload for piping into external signers.

See docs/c2pa_sigma_credential.md.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Any, Dict


def load_sigma_sidecar(path: str | Path) -> Dict[str, Any]:
    """Load a `*.cos.json` file produced by Creation OS."""
    p = Path(path)
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)


def add_sigma_assertion(asset_path: str, sigma_data: Dict[str, Any]) -> None:
    """
    Hook for merging `sigma_data` into a C2PA manifest for `asset_path`.

    Install the official C2PA Python toolkit for your target environment and
    map `sigma_data` to a custom assertion labelled `ai.creation-os.sigma`.
    This repository does not pin a particular `c2pa` wheel API here.
    """
    try:
        import c2pa  # noqa: F401
    except ImportError:
        print(
            "c2pa_integration: optional `c2pa` package not installed; "
            "sidecar JSON is still valid for out-of-band tooling.",
            file=sys.stderr,
        )
        return
    print(
        "c2pa_integration: `c2pa` is importable — wire Builder/Manifest APIs "
        "from your installed version to attach label ai.creation-os.sigma.\n"
        f"  asset={asset_path!r} keys={list(sigma_data.keys())!r}",
        file=sys.stderr,
    )


def main(argv: list[str]) -> int:
    if len(argv) < 3:
        print(
            "usage: python3 tools/c2pa_integration.py <asset> <sidecar.cos.json>",
            file=sys.stderr,
        )
        return 2
    asset = argv[1]
    sidecar = argv[2]
    data = load_sigma_sidecar(sidecar)
    add_sigma_assertion(asset, data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

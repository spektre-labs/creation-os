# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path

from cos.offline import SigmaOffline


def test_package_manifest_sha256() -> None:
    o = SigmaOffline()
    b = o.package(
        model={"fmt": "GGUF"},
        gate={"mode": "lite"},
        probes={},
        codex={"rev": 1},
    )
    assert "sha256" in b and len(b["sha256"]) == 64


def test_verify_bundle_round_trip() -> None:
    o = SigmaOffline()
    b = o.package({"m": 1}, {"g": 1}, {}, {})
    with tempfile.TemporaryDirectory() as td:
        path = o.write_bundle(b, Path(td))
        vr = o.verify_bundle(path)
        assert vr["ok"] is True


def test_verify_bundle_detects_tamper() -> None:
    o = SigmaOffline()
    b = o.package({"m": 1}, {"g": 1}, {}, {})
    with tempfile.TemporaryDirectory() as td:
        path = o.write_bundle(b, Path(td))
        data = json.loads(path.read_text(encoding="utf-8"))
        pl = data.get("payload") or {}
        if isinstance(pl, dict):
            pl["model"] = {"m": 999}
        data["payload"] = pl
        path.write_text(json.dumps(data), encoding="utf-8")
        vr = o.verify_bundle(path)
        assert vr["ok"] is False


def test_deploy_airgap() -> None:
    o = SigmaOffline()
    b = o.package({}, {}, {}, {})
    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "b.json"
        src.write_text(json.dumps(b), encoding="utf-8")
        with tempfile.TemporaryDirectory() as td2:
            r = o.deploy_airgap(src, Path(td2), verify=False)
            assert r["deployed"] is True


def test_no_network_check_skipped_in_ci() -> None:
    o = SigmaOffline()
    os.environ["COS_FORCE_AIRGAP_OK"] = "1"
    try:
        r = o.no_network_check()
        assert r.get("ok_for_airgap_policy") is True
    finally:
        os.environ.pop("COS_FORCE_AIRGAP_OK", None)


def test_bundle_trust_sigma() -> None:
    o = SigmaOffline()
    b = o.package({"a": 1}, {}, {}, {})
    t = o.bundle_trust_sigma(b)
    assert "sigma" in t and "verdict" in t

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.plugin`."""
from __future__ import annotations

import tempfile

import pytest

from cos.marketplace import SigmaMarketplace
from cos.plugin import SigmaPlugin


def test_register_and_list() -> None:
    plug = SigmaPlugin()

    def cb(_: str) -> str:
        return "'sandbox_ok'"

    reg = plug.register("t1", "pre_score", cb)
    assert "sigma" in reg["sigma_profile"]
    names = [r["name"] for r in plug.list_plugins()]
    assert "t1" in names


def test_register_invalid_hook() -> None:
    plug = SigmaPlugin()
    with pytest.raises(ValueError, match="hook_point"):
        plug.register("x", "between_scores", lambda x: x)


def test_disable() -> None:
    plug = SigmaPlugin()
    plug.register("d1", "on_accept", lambda: None)
    assert plug.disable("d1")["ok"] is True
    row = next(x for x in plug.list_plugins() if x["name"] == "d1")
    assert row["disabled"] is True


def test_sandbox_execute() -> None:
    plug = SigmaPlugin()
    r = plug.sandbox_execute({"callback": lambda s: "1+2"}, "ctx")
    assert r.get("ok") is True
    assert r.get("result") == 3


def test_marketplace_install() -> None:
    m = SigmaMarketplace()
    pub = m.publish_probe({"id": "probe_a"}, {}, {})
    plug = SigmaPlugin()
    ins = plug.marketplace_install(pub["probe_id"], m)
    assert ins["ok"] is True
    assert any(p["name"] == ins["name"] for p in plug.list_plugins())


def test_load_from_module() -> None:
    plug = SigmaPlugin()
    src = """
def register_sigma_plugin(p):
    p.register("from_file", "pre_cascade", lambda x: "1")
"""
    with tempfile.NamedTemporaryFile(mode="w", suffix=".py", delete=False) as f:
        f.write(src)
        path = f.name
    try:
        r = plug.load_from_module(path)
        assert r["ok"] is True
        assert any(x["name"] == "from_file" for x in plug.list_plugins())
    finally:
        import os

        os.unlink(path)

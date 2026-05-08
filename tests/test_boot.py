# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from unittest.mock import patch

from cos.boot import Boot


def test_boot_returns_steps() -> None:
    b = Boot()
    r = b.run()
    assert r["booted"] is True
    st = r["steps"]
    for name in ("kernel", "identity", "fabric", "mega", "cascade"):
        assert name in st
    assert "loaded" in r and "total" in r


def test_boot_kernel_always_ok() -> None:
    r = Boot().run()
    assert r["steps"]["kernel"]["status"] == "OK"
    assert "σ" in r["steps"]["kernel"]


def test_boot_partial_graceful() -> None:
    with patch("cos.mega.Mega", side_effect=RuntimeError("injected Mega failure")):
        r = Boot().run()
    assert r["steps"]["mega"]["status"] == "SKIP"
    assert "reason" in r["steps"]["mega"]
    assert r["steps"]["kernel"]["status"] == "OK"
    assert r["ready"] is True

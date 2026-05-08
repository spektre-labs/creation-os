# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import builtins

import pytest


def test_no_nicegui_raises(monkeypatch: pytest.MonkeyPatch) -> None:
    import importlib
    import sys

    real_import = builtins.__import__

    def _block(name: str, globals=None, locals=None, fromlist=(), level=0):  # noqa: ANN001
        root = str(name).split(".")[0]
        if root == "nicegui":
            raise ImportError("simulated missing nicegui")
        return real_import(name, globals, locals, fromlist, level)

    monkeypatch.setattr(builtins, "__import__", _block)
    for key in list(sys.modules.keys()):
        if key == "nicegui" or key.startswith("nicegui."):
            sys.modules.pop(key, None)
    sys.modules.pop("cos.ui.dashboard", None)
    sys.modules.pop("cos.ui", None)
    importlib.invalidate_caches()
    dash = importlib.import_module("cos.ui.dashboard")
    with pytest.raises(ImportError, match=r"creation-os\[ui\]|pip install|nicegui"):
        dash.create_dashboard()
    sys.modules.pop("cos.ui.dashboard", None)
    sys.modules.pop("cos.ui", None)


@pytest.mark.skipif(
    __import__("importlib.util").util.find_spec("nicegui") is None,
    reason="nicegui not installed",
)
def test_create_dashboard_importable() -> None:
    from cos.ui.dashboard import create_dashboard

    ui_mod = create_dashboard()
    assert ui_mod is not None
    assert hasattr(ui_mod, "run")


def test_ui_optional_dependency() -> None:
    import cos.ui as ui_pkg

    assert hasattr(ui_pkg, "create_dashboard")
    assert hasattr(ui_pkg, "run_dashboard")
    assert hasattr(ui_pkg, "HAS_UI")

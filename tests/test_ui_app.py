# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import builtins

import pytest


def test_create_ui_without_nicegui_raises(monkeypatch) -> None:
    import importlib
    import sys

    real_import = builtins.__import__

    def _block(name: str, globals=None, locals=None, fromlist=(), level=0):
        if name == "nicegui" or name.split(".")[0] == "nicegui":
            raise ImportError("simulated missing nicegui")
        return real_import(name, globals, locals, fromlist, level)

    monkeypatch.setattr(builtins, "__import__", _block)
    sys.modules.pop("nicegui", None)
    sys.modules.pop("cos.ui.app", None)
    appmod = importlib.import_module("cos.ui.app")

    with pytest.raises(ImportError, match=r"creation-os\[ui\]"):
        appmod.build_app()


@pytest.mark.skipif(
    __import__("importlib.util").util.find_spec("nicegui") is None,
    reason="nicegui not installed",
)
def test_sigma_gauge_updates_from_gate_score() -> None:
    from cos.sigma_gate import SigmaGate
    from cos.ui.app import verdict_to_color

    g = SigmaGate()
    sigma, verdict = g.score("user prompt", "stable short assistant reply")
    vn = verdict if isinstance(verdict, str) else str(verdict).rsplit(".", 1)[-1]
    assert 0.0 <= float(sigma) <= 1.0
    assert verdict_to_color(vn).startswith("#")


def test_verdict_colors() -> None:
    from cos.ui.app import verdict_to_color

    assert verdict_to_color("ACCEPT") == "#16a34a"
    assert verdict_to_color("RETHINK") == "#ea580c"
    assert verdict_to_color("ABSTAIN") == "#dc2626"


def test_threshold_sliders_bind_tau() -> None:
    from cos.sigma_gate import SigmaGate
    from cos.ui.app import bind_gate_tau_from_sliders

    class _S:
        value: float

        def __init__(self, v: float) -> None:
            self.value = v

        def on_value_change(self, cb) -> None:
            self._cb = cb

        def fire(self, v: float) -> None:
            self.value = v
            self._cb(None)

    g = SigmaGate()
    a, b = _S(0.2), _S(0.85)
    bind_gate_tau_from_sliders(a, b, g)
    a.fire(0.11)
    b.fire(0.88)
    assert g.tau_accept == pytest.approx(0.11)
    assert g.tau_abstain == pytest.approx(0.88)


def test_evidence_ladder_shows_negatives() -> None:
    from cos.ui.app import EVIDENCE_LADDER, NOT_AGI_BANNER

    assert NOT_AGI_BANNER == "NOT AGI ACHIEVED"
    assert any("fail" in row[2].lower() for row in EVIDENCE_LADDER)


@pytest.mark.skipif(
    __import__("importlib.util").util.find_spec("nicegui") is None,
    reason="nicegui not installed",
)
def test_build_app_returns_ui_module() -> None:
    from nicegui import ui as nu

    from cos.ui.app import SigmaUIConfig, build_app

    u = build_app(SigmaUIConfig(title="t"))
    assert u is nu


@pytest.mark.skipif(
    __import__("importlib.util").util.find_spec("nicegui") is None,
    reason="nicegui not installed",
)
def test_run_ui_invokes_nicegui_run(monkeypatch) -> None:
    import nicegui

    buf: list[dict] = []
    monkeypatch.setattr(nicegui.ui, "run", lambda **kw: buf.append(kw))
    from cos.ui.app import run_ui

    run_ui(port=9999, show=False)
    assert buf and buf[-1]["port"] == 9999

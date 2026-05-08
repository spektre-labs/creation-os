# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""NiceGUI σ-dashboard — live gate score, σ trace (ECharts), Fabric L0–L9 + module status.

Optional dependency: ``pip install 'creation-os[ui]'``. Local-first defaults (127.0.0.1).
See ``docs/CLAIM_DISCIPLINE.md`` — **NOT AGI ACHIEVED**."""
from __future__ import annotations

import os
from typing import Any, List, Optional

__all__ = [
    "HAS_UI",
    "create_dashboard",
    "run_dashboard",
]

try:  # pragma: no cover - exercised when nicegui installed
    import nicegui  # noqa: F401

    HAS_UI = True
except ImportError:  # pragma: no cover
    HAS_UI = False


def _verdict_token(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


def create_dashboard(gate: Optional[Any] = None) -> Any:
    """Register the σ-monitoring dashboard page; returns the NiceGUI ``ui`` module."""
    if not HAS_UI:
        raise ImportError("pip install 'creation-os[ui]' (nicegui not found)")

    from nicegui import ui

    from cos.sigma_gate import SigmaGate
    from cos.ui.app import verdict_to_color

    g = gate if gate is not None else SigmaGate()
    sigma_history: List[float] = []

    @ui.page("/")
    def dashboard_page() -> None:
        chart: Any = None

        with ui.header().classes("bg-blue-900 text-white px-4 py-3"):
            ui.label("CREATION OS").classes("text-2xl font-bold")
            ui.label("σ-AWARE ARCHITECTURE").classes("text-sm opacity-70")

        with ui.row().classes("w-full gap-4 p-4 flex-wrap"):
            with ui.card().classes("w-full min-w-[18rem] max-w-md flex-1"):
                ui.label("σ-Gate Score").classes("text-lg font-bold")
                prompt_input = ui.input("Prompt", value="What is 2+2?")
                response_input = ui.input("Response", value="4")
                sigma_label = ui.label("σ = —").classes("font-mono")
                verdict_label = ui.label("Verdict: —")

                def score() -> None:
                    sigma, verdict = g.score(str(prompt_input.value or ""), str(response_input.value or ""))
                    sigma_history.append(float(sigma))
                    vn = _verdict_token(verdict)
                    sigma_label.text = f"σ = {float(sigma):.4f}"
                    verdict_label.text = f"Verdict: {vn}"
                    col = verdict_to_color(vn)
                    verdict_label.style(f"color: {col}; font-weight: 600")
                    if chart is not None:
                        tail = sigma_history[-50:]
                        chart.options["xAxis"]["data"] = [str(i + 1) for i in range(len(tail))]
                        chart.options["series"][0]["data"] = tail
                        chart.update()

                ui.button("Score", on_click=score).classes("bg-blue-800 text-white mt-2")

            with ui.card().classes("w-full min-w-[18rem] max-w-md flex-1"):
                ui.label("σ Trace").classes("text-lg font-bold")
                chart = ui.echart(
                    {
                        "xAxis": {"type": "category", "data": []},
                        "yAxis": {"type": "value", "min": 0, "max": 1},
                        "series": [
                            {
                                "type": "line",
                                "data": [],
                                "smooth": True,
                                "areaStyle": {"opacity": 0.3},
                            }
                        ],
                        "grid": {"left": "3%", "right": "4%", "bottom": "3%", "containLabel": True},
                    }
                ).classes("h-64 w-full")

            with ui.card().classes("w-full min-w-[18rem] max-w-md flex-1"):
                ui.label("System Status (Fabric)").classes("text-lg font-bold")
                layer_box = ui.column().classes("gap-1 max-h-48 overflow-y-auto")
                module_box = ui.column().classes("gap-1 max-h-48 overflow-y-auto")
                observe_label = ui.label("Observe: —").classes("text-sm opacity-90")

                def refresh_fabric() -> None:
                    layer_box.clear()
                    module_box.clear()
                    try:
                        from cos.fabric import FABRIC_LAYER_MAP, Fabric

                        fab = Fabric()
                        fab.boot()
                        layers = fab.layer_status()
                        with layer_box:
                            ui.label("L0–L9 coverage").classes("text-xs font-bold opacity-70")
                            for layer_name in sorted(FABRIC_LAYER_MAP.keys()):
                                info = layers.get(layer_name) or {}
                                cov = float(info.get("coverage") or 0.0)
                                ui.label(f"{layer_name}: coverage {cov:.0%}").classes(
                                    "text-xs" + (" text-green-700" if cov >= 0.5 else " text-amber-800")
                                )
                        st = fab.status()
                        mods = st.get("modules") or {}
                        with module_box:
                            ui.label("Modules").classes("text-xs font-bold opacity-70")
                            for name in sorted(mods.keys()):
                                info2 = mods[name]
                                state = str(info2.get("state") or "unknown")
                                icon = "✓" if state == "loaded" else "✗"
                                col = "green" if state == "loaded" else "red"
                                ui.label(f"{icon} {name}: {state}").style(f"color: {col}; font-size: 12px")
                        ob = fab.get("observe")
                        observe_label.text = (
                            f"Observe: {type(ob).__name__} ({'ok' if ob is not None else 'missing'})"
                        )
                    except Exception as exc:  # noqa: BLE001
                        with layer_box:
                            ui.label(f"Fabric: {exc}").classes("text-red-600 text-sm")

                refresh_fabric()
                ui.timer(15.0, refresh_fabric)

        with ui.footer().classes("bg-gray-900 text-center py-2"):
            ui.label("NOT AGI ACHIEVED · 1=1 · σ-AWARE").classes("text-xs opacity-50")

    return ui


def run_dashboard(
    host: str = "127.0.0.1",
    port: int = 8080,
    *,
    title: str = "Creation OS σ-Dashboard",
    show: bool = True,
    native: bool = False,
    gate: Optional[Any] = None,
) -> None:
    """Build pages and start the NiceGUI event loop (blocking)."""
    ui_mod = create_dashboard(gate=gate)
    port = int(os.environ.get("COS_UI_PORT", port))
    host = str(os.environ.get("COS_UI_HOST", host))
    ui_mod.run(host=host, port=int(port), title=title, show=show, native=native)

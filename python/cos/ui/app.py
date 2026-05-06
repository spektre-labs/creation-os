# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Creation OS NiceGUI shell — σ-gauge, chat, evidence ladder (optional ``nicegui`` / ``pywebview``)."""
from __future__ import annotations

import os
from typing import Any, Callable, List, Optional, Tuple

__all__ = [
    "SigmaUIConfig",
    "NOT_AGI_BANNER",
    "EVIDENCE_LADDER",
    "verdict_to_color",
    "bind_gate_threshold_from_sliders",
    "bind_gate_tau_from_sliders",
    "build_app",
    "create_ui",
    "run_ui",
]

NOT_AGI_BANNER = "NOT AGI ACHIEVED"

# Evidence ladder (lab positioning); negative rows stay visible per productization brief.
EVIDENCE_LADDER: List[Tuple[str, str, str]] = [
    ("TruthfulQA AUROC", "0.982", "saturated"),
    ("TriviaQA AUROC", "0.960", "ok"),
    ("HaluEval AUROC", "0.514", "fail"),
]


def verdict_to_color(verdict: str) -> str:
    """CSS color for gate verdict (Tailwind-agnostic)."""
    v = verdict if isinstance(verdict, str) else str(verdict).rsplit(".", 1)[-1]
    if v == "ACCEPT":
        return "#16a34a"
    if v == "RETHINK":
        return "#ea580c"
    return "#dc2626"


def bind_gate_threshold_from_sliders(
    accept_slider: Any,
    abstain_slider: Any,
    gate: Any,
) -> None:
    """Wire NiceGUI sliders to mutate ``gate.threshold_accept`` / ``gate.threshold_abstain``."""

    def _sync_accept(_: Any) -> None:
        gate.threshold_accept = float(accept_slider.value)

    def _sync_abstain(_: Any) -> None:
        gate.threshold_abstain = float(abstain_slider.value)

    accept_slider.on_value_change(_sync_accept)
    abstain_slider.on_value_change(_sync_abstain)


bind_gate_tau_from_sliders = bind_gate_threshold_from_sliders


class SigmaUIConfig:
    def __init__(self, *, title: str = "Creation OS — σ UI") -> None:
        self.title = title


def create_ui(
    gate: Any = None,
    chat_fn: Optional[Callable[[str], str]] = None,
    endpoint: Optional[str] = None,
    config: Optional[SigmaUIConfig] = None,
) -> Any:
    """Build the desktop UI (alias of :func:`build_app` with explicit gate/chat hooks)."""
    return build_app(config, gate=gate, chat_fn=chat_fn, endpoint=endpoint)


def build_app(
    config: Optional[SigmaUIConfig] = None,
    *,
    gate: Any = None,
    chat_fn: Optional[Callable[[str], str]] = None,
    endpoint: Optional[str] = None,
) -> Any:
    """Construct NiceGUI app; raises ``ImportError`` if ``nicegui`` is missing."""
    del endpoint  # reserved for future HTTP chat backends
    try:
        from nicegui import ui  # type: ignore[import-not-found]
    except ImportError as exc:  # pragma: no cover
        raise ImportError(
            "cos ui needs optional deps: pip install 'creation-os[ui]'  ({})".format(exc)
        ) from exc

    cfg = config or SigmaUIConfig()
    from cos.sigma_gate import SigmaGate

    gate = gate or SigmaGate()

    @ui.page("/")
    def home() -> None:
        with ui.header().style("background-color: #1e3a8a; color: white; padding: 12px 16px"):
            ui.label(cfg.title).classes("text-h5 text-weight-bold")
            ui.label("σ-gate cognitive architecture").classes("text-caption")

        with ui.left_drawer(value=False).classes("bg-grey-2").style("width: 280px"):
            ui.label("σ configuration").classes("text-subtitle1 text-weight-bold")
            accept_slider = ui.slider(min=0.0, max=1.0, step=0.01, value=gate.threshold_accept)
            ui.label("Accept τ (lower σ = calmer)")
            abstain_slider = ui.slider(min=0.0, max=1.0, step=0.01, value=gate.threshold_abstain)
            ui.label("Abstain τ")
            bind_gate_threshold_from_sliders(accept_slider, abstain_slider, gate)

            ui.separator()
            ui.label("Evidence (lab)").classes("text-subtitle1 text-weight-bold")
            for name, val, note in EVIDENCE_LADDER:
                color = "#b91c1c" if "fail" in note.lower() else ("#ca8a04" if "saturated" in note.lower() else "#15803d")
                ui.label(f"{name}: {val} — {note}").style(f"color: {color}; font-size: 13px")
            ui.separator()
            ui.label(NOT_AGI_BANNER).classes("text-subtitle2 text-weight-bold").style("color: #b91c1c")

        with ui.column().classes("w-full").style("max-width: 48rem; margin: 0 auto; padding: 16px"):
            current_sigma = 0.0
            sigma_label = ui.label(f"σ = {current_sigma:.3f}").classes("text-h4").style("font-family: monospace")
            sigma_bar = ui.linear_progress(value=0.0).classes("w-full")
            verdict_label = ui.label("—").classes("text-h6")

            ui.separator()
            chat_container = ui.column()
            chat_container.classes("w-full gap-2")

            user_input = ui.input(placeholder="Ask anything…").classes("w-full")
            send_btn = ui.button("Send")

            def send_message() -> None:
                nonlocal current_sigma
                text = (user_input.value or "").strip()
                if not text:
                    return
                user_input.value = ""
                with chat_container:
                    ui.chat_message(text, name="You", sent=True)
                if chat_fn is not None:
                    response = chat_fn(text)
                else:
                    response = f"[No model connected] Echo: {text}"
                sigma, verdict = gate.score(text, response)
                vn = verdict.name if hasattr(verdict, "name") else str(verdict).rsplit(".", 1)[-1]
                current_sigma = float(sigma)
                sigma_label.text = f"σ = {current_sigma:.3f}"
                sigma_bar.value = min(1.0, max(0.0, current_sigma))
                col = verdict_to_color(vn)
                verdict_label.text = str(vn)
                verdict_label.style(f"color: {col}")
                with chat_container:
                    ui.chat_message(
                        f"[σ={current_sigma:.3f} {vn}]\n{response}",
                        name="Creation OS",
                        sent=False,
                    ).style(f"border-left: 4px solid {col}; padding-left: 8px")

            send_btn.on_click(send_message)

    return ui


def run_ui(
    *,
    host: str = "127.0.0.1",
    port: int = 8765,
    show: bool = True,
    native: bool = False,
    config: Optional[SigmaUIConfig] = None,
    gate: Any = None,
    chat_fn: Optional[Callable[[str], str]] = None,
    endpoint: Optional[str] = None,
) -> None:
    """Start the NiceGUI event loop (blocking)."""
    cfg = config or SigmaUIConfig()
    uimod = build_app(cfg, gate=gate, chat_fn=chat_fn, endpoint=endpoint)
    port = int(os.environ.get("COS_UI_PORT", port))
    host = os.environ.get("COS_UI_HOST", host)
    uimod.run(host=host, port=port, show=show, native=native, title=cfg.title)

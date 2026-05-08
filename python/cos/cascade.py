# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""L2–L6 cascade signals for :meth:`~cos.sigma_gate.SigmaGate.score_cascade`.

Layers map to probe families (lab): L2=HIDE, L3=ICR (cross-layer updates), L4=SEP/energy
on last hidden, L5=spectral / stack structure, **L6=sink+spectral (+ optional compression)**. See ``docs/CLAIM_DISCIPLINE.md``.

For **multi-model cost routing** (FAST → VERIFY → RAG → ESCALATE), see ``cos.cascade_router``."""
from __future__ import annotations

from typing import Any

from cos.probe import EnergyProbe, ICRProbe, SEPProbe, SignalCascade, SpectralProbe
from cos.sink_probe import SigmaSinkProbe

_sc = SignalCascade()
_icr = ICRProbe()
_sep = SEPProbe()
_energy = EnergyProbe()
_spec = SpectralProbe()
_sink = SigmaSinkProbe()


def cascade_L2(hidden_states: Any) -> float:
    """Consecutive-layer representation drift (HIDE-style)."""
    hs = _sc.as_layer_list(hidden_states)
    return float(_sc.hide_score(hs))


def cascade_L3(hidden_states: Any) -> float:
    """Cross-layer update / ICR-style signal (not a single frozen layer index)."""
    return float(_icr.score(hidden_states))


def cascade_L4(hidden_states: Any) -> float:
    """SEP surrogate on last hidden + energy fallback (no separate logits in score_cascade)."""
    hs = _sc.as_layer_list(hidden_states)
    if not hs:
        return 0.5
    sep = float(_sep.score_hidden_tensor(hs[-1]))
    eng = float(_energy.score_hidden_tensor(hs[-1]))
    return float(0.6 * sep + 0.4 * eng)


def cascade_L5(hidden_states: Any) -> float:
    """Spectral / low–effective-rank cue over the layer stack (attention optional elsewhere)."""
    return float(_spec.score_hidden_stack(hidden_states))


def cascade_L6(attention_maps: Any, hidden_states: Any = None) -> float:
    """Attention sink + spectral cue; optional mid-layer compression from ``hidden_states``."""
    if attention_maps is None:
        return 0.0
    c = _sink.combined_with_spectral(attention_maps)
    val = float(c["combined"])
    if hidden_states is not None:
        cv = _sink.compression_valley_detect(hidden_states)
        val = 0.85 * val + 0.15 * float(cv["score"])
    return float(min(1.0, max(0.0, val)))


__all__ = ["cascade_L2", "cascade_L3", "cascade_L4", "cascade_L5", "cascade_L6"]

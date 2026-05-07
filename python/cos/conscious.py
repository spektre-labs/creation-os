# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Metacognition / awareness-metric façade (lab).

Implements :class:`SigmaConscious` as a thin entry point over :class:`SigmaConsciousProxy`.
This module is internal wiring; user-facing docs and CLI refer to *metacognition* /
*awareness_metrics*, not colloquial “consciousness.” See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict

from cos.sigma_conscious import SigmaConsciousProxy
from cos.sigma_gate import SigmaGate

__all__ = ["SigmaConscious"]


class _Stub:
    """Optional ports for :class:`SigmaConsciousProxy` when only a gate is wired."""


class SigmaConscious:
    """Thin wrapper so :mod:`cos.fabric` can boot metacognition / awareness-metric proxies with ``gate=`` only."""

    def __init__(self, gate: Any = None) -> None:
        g = gate if gate is not None else SigmaGate()
        self._proxy = SigmaConsciousProxy(g, _Stub(), _Stub(), _Stub())

    def σ_meta(self) -> Dict[str, Any]:
        return self._proxy.measure_consciousness_proxies()

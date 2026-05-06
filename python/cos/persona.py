# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-persona — deployment-context σ thresholds (lab).

Tie thresholds to automotive / medical / etc. **policies** are UX hints only. Integration:
``automotive.py``, ``device.py``, ``constitution.py``, ``serve.py`` query param ``persona``."""
from __future__ import annotations

from typing import Any, Dict, Mapping, Optional, Tuple

__all__ = ["SigmaPersona"]


class SigmaPersona:
    """Named personas: ``threshold_accept`` / ``threshold_abstain`` + latency budgets + policy blurbs."""

    def __init__(self) -> None:
        self.personas: Dict[str, Dict[str, Any]] = {
            "automotive": {
                "threshold_accept": 0.15,
                "threshold_abstain": 0.5,
                "latency_max_ms": 50,
            },
            "medical": {
                "threshold_accept": 0.1,
                "threshold_abstain": 0.4,
                "latency_max_ms": 200,
            },
            "creative": {
                "threshold_accept": 0.5,
                "threshold_abstain": 0.9,
                "latency_max_ms": 1000,
            },
            "enterprise": {
                "threshold_accept": 0.2,
                "threshold_abstain": 0.6,
                "latency_max_ms": 500,
            },
            "research": {
                "threshold_accept": 0.3,
                "threshold_abstain": 0.8,
                "latency_max_ms": 2000,
            },
        }
        self._active: Optional[str] = None

    def activate(self, persona_name: str) -> Dict[str, Any]:
        """Select persona (in-process hint); use ``apply_gate_overrides`` to change a ``SigmaGate``."""
        name = str(persona_name)
        if name not in self.personas:
            raise KeyError(f"unknown persona: {name}")
        self._active = name
        return dict(self.personas[name])

    def custom(self, name: str, thresholds: Mapping[str, float], constraints: Mapping[str, Any]) -> Dict[str, Any]:
        """Register operator persona: ``thresholds`` must include accept/abstain τ."""
        spec = {
            "threshold_accept": float(thresholds["threshold_accept"]),
            "threshold_abstain": float(thresholds["threshold_abstain"]),
            **{k: constraints[k] for k in constraints},
        }
        self.personas[str(name)] = spec
        return {"name": str(name), "spec": spec}

    @staticmethod
    def sigma_policy(persona: str) -> Dict[str, str]:
        """What ACCEPT / RETHINK / ABSTAIN **mean** in UX terms for built-in personas."""
        p = str(persona).lower().strip()
        table = {
            "automotive": {
                "ACCEPT": "Response is consistent with vehicle constraints.",
                "RETHINK": "Re-check against OEM documentation snippets.",
                "ABSTAIN": "I do not know — consult the owner's manual or dealer.",
            },
            "medical": {
                "ACCEPT": "Informational only; not a diagnosis.",
                "RETHINK": "Verify with a qualified clinician before acting.",
                "ABSTAIN": "Cannot provide medical advice — see a doctor.",
            },
            "creative": {
                "ACCEPT": "Draft is stylistically coherent.",
                "RETHINK": "This is one creative option among many.",
                "ABSTAIN": "Prefer not to generate this variation.",
            },
            "enterprise": {
                "ACCEPT": "Fits internal policy and cited sources.",
                "RETHINK": "Escalate to compliance or legal if unsure.",
                "ABSTAIN": "Insufficient policy grounding to answer.",
            },
            "research": {
                "ACCEPT": "Caveated claim with traceable reasoning.",
                "RETHINK": "Needs stronger citations or replication note.",
                "ABSTAIN": "Evidence too thin — do not cite as fact.",
            },
        }
        if p not in table:
            return {
                "ACCEPT": "Meets persona threshold_accept.",
                "RETHINK": "Between threshold_accept and threshold_abstain — review.",
                "ABSTAIN": "Above threshold_abstain — withhold or block.",
            }
        return table[p]

    @staticmethod
    def context_detect(input_metadata: Mapping[str, Any]) -> str:
        """Pick persona from coarse metadata (``domain``, ``device_type``, ``regulated``)."""
        meta = {k.lower(): v for k, v in input_metadata.items()}
        domain = str(meta.get("domain") or "").lower()
        device = str(meta.get("device_type") or meta.get("device") or "").lower()
        regulated = bool(meta.get("regulated") or meta.get("hipaa") or meta.get("clinical"))
        if regulated or domain == "medical" or device == "clinical_workstation":
            return "medical"
        if domain in ("automotive", "adas", "vehicle") or device in ("ivihu", "telematics"):
            return "automotive"
        if domain in ("creative", "marketing", "story"):
            return "creative"
        if domain in ("research", "lab"):
            return "research"
        return "enterprise"

    def apply_gate_overrides(self, gate: Any, persona_name: str) -> Optional[Tuple[float, float]]:
        """Temporarily set gate thresholds; returns previous ``(threshold_accept, threshold_abstain)`` or ``None``."""
        spec = self.personas.get(str(persona_name))
        if not spec:
            return None
        old = (float(gate.threshold_accept), float(gate.threshold_abstain))
        gate.threshold_accept = float(spec["threshold_accept"])
        gate.threshold_abstain = float(spec["threshold_abstain"])
        self._active = str(persona_name)
        return old

    @staticmethod
    def restore_gate(gate: Any, previous: Tuple[float, float]) -> None:
        gate.threshold_accept, gate.threshold_abstain = float(previous[0]), float(previous[1])

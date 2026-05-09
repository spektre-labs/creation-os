# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Cognitive twin lab: compare a **shadow state** to observed reality with σ on paired strings.

This is a **toy** alignment shell for “sim vs telemetry” narratives — not a certified digital-twin
platform, physics co-simulation, or guarantee that :class:`CognitiveTwin` mirrors any production
asset. Optional :mod:`cos.world` can act as a richer *internal* simulator in full trees; here the
twin state is an explicit dict you update. See ``docs/CLAIM_DISCIPLINE.md``. ``sigma_gate.h`` is
not modified by this module.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["CognitiveTwin"]


class CognitiveTwin:
    """Lab twin: ``twin_state`` vs ``real_state`` scored with :class:`~cos.sigma_gate.SigmaGate`."""

    def __init__(self, gate: Optional[Any] = None, name: str = "twin") -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.name = str(name)
        self.twin_state: Dict[str, Dict[str, Any]] = {}
        self.real_state: Dict[str, Dict[str, Any]] = {}
        self.sigma_history: List[float] = []
        self.sync_count = 0
        self.drift_events: List[Dict[str, Any]] = []

    def observe_real(self, key: str, value: Any) -> None:
        self.real_state[str(key)] = {"value": value, "timestamp": time.time()}

    def predict_twin(self, key: str) -> Any:
        return self.twin_state.get(str(key), {}).get("value")

    def sync(self) -> Dict[str, Any]:
        if not self.real_state:
            return {"synced": False, "reason": "no real state"}

        sigma_total = 0.0
        updates = 0
        drifts: List[Dict[str, Any]] = []

        for key, real in self.real_state.items():
            twin_val = self.predict_twin(key)
            sigma, _verdict = self.gate.score(
                f"twin predicts {key}={twin_val}",
                f"reality shows {key}={real['value']}",
            )
            sigma = float(sigma)
            sigma_total += sigma

            sk = str(key)
            if sigma > 0.3:
                self.twin_state[sk] = {
                    "value": real["value"],
                    "σ_at_update": sigma,
                    "timestamp": time.time(),
                }
                updates += 1
                drifts.append({"key": sk, "σ": round(sigma, 4)})
            elif sk not in self.twin_state:
                self.twin_state[sk] = {
                    "value": real["value"],
                    "σ_at_update": sigma,
                    "timestamp": time.time(),
                }
                updates += 1

        avg_sigma = sigma_total / float(max(len(self.real_state), 1))
        self.sigma_history.append(avg_sigma)
        self.sync_count += 1
        if drifts:
            self.drift_events.extend(drifts)

        return {
            "synced": True,
            "avg_σ": round(avg_sigma, 4),
            "updates": updates,
            "drifts": drifts,
            "fidelity": round(1.0 - avg_sigma, 4),
            "sync_number": self.sync_count,
        }

    def what_if(self, key: str, hypothetical_value: Any) -> Dict[str, Any]:
        """Contrast σ for “twin vs current real” vs “twin vs hypothetical real” for one key (lab)."""
        sk = str(key)
        twin_val = self.predict_twin(sk)
        cur_real = self.real_state.get(sk, {}).get("value")
        cur_sigma, _ = self.gate.score(
            f"twin predicts {sk}={twin_val}",
            f"reality shows {sk}={cur_real}",
        )
        hypo_sigma, _ = self.gate.score(
            f"twin predicts {sk}={twin_val}",
            f"reality shows {sk}={hypothetical_value}",
        )
        cur_sigma = float(cur_sigma)
        hypo_sigma = float(hypo_sigma)
        delta = abs(hypo_sigma - cur_sigma)
        if hypo_sigma < cur_sigma - 1e-9:
            rec = "change is beneficial"
        elif delta < 0.05:
            rec = "change is neutral"
        else:
            rec = "change is risky"

        return {
            "key": sk,
            "current_value": cur_real,
            "hypothetical_value": hypothetical_value,
            "σ_current": round(cur_sigma, 4),
            "σ_hypothetical": round(hypo_sigma, 4),
            "impact": round(delta, 4),
            "recommendation": rec,
        }

    def predictive_alert(self, threshold: float = 0.6) -> Dict[str, Any]:
        if len(self.sigma_history) < 5:
            return {"alert": False, "reason": "insufficient history"}

        recent = self.sigma_history[-5:]
        trend = recent[-1] - recent[0]

        if trend > 0.1 and recent[-1] > threshold * 0.7:
            return {
                "alert": True,
                "severity": "HIGH" if recent[-1] > threshold else "MEDIUM",
                "trend": round(trend, 4),
                "current_σ": round(recent[-1], 4),
                "predicted_breach": "imminent" if trend > 0.2 else "soon",
                "action": "sync twin with reality NOW",
            }

        return {
            "alert": False,
            "trend": round(trend, 4),
            "current_σ": round(recent[-1], 4),
        }

    def fidelity_report(self) -> Dict[str, Any]:
        if not self.sigma_history:
            return {"fidelity": 0, "syncs": 0}

        avg_sigma = sum(self.sigma_history) / float(len(self.sigma_history))
        return {
            "name": self.name,
            "fidelity": round(1.0 - self.sigma_history[-1], 4),
            "avg_fidelity": round(1.0 - avg_sigma, 4),
            "syncs": self.sync_count,
            "total_drifts": len(self.drift_events),
            "twin_keys": len(self.twin_state),
            "real_keys": len(self.real_state),
            "coverage": round(
                len(self.twin_state) / float(max(len(self.real_state), 1)),
                4,
            ),
        }

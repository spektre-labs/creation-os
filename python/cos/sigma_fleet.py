# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-fleet — register Creation OS inference devices, route by σ health, sync calibration metadata.

**Local-first:** devices marked ``local: true`` participate in ``privacy="high"`` routing.

This is a **lab registry** (in-memory / JSON file optional via CLI); it is not a distributed
control plane by itself.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional


class SigmaFleet:
    """Maintain a dict of devices with σ calibration metadata and simple routing."""

    def __init__(self) -> None:
        self.devices: Dict[str, Dict[str, Any]] = {}

    def register(self, device_id: str, capabilities: Dict[str, Any]) -> None:
        did = str(device_id)
        cap = dict(capabilities)
        self.devices[did] = {
            "id": did,
            "capabilities": cap,
            "model_version": cap.get("model_version") or cap.get("model"),
            "sigma_calibration": cap.get("calibration_date") or cap.get("sigma_calibration"),
            "avg_sigma": float(cap.get("avg_sigma", 0.5)),
            "status": str(cap.get("status", "active")),
        }

    def route_to_best(self, _prompt: str, *, privacy: str = "normal") -> Optional[Dict[str, Any]]:
        """Pick lowest ``avg_sigma`` among active devices (tie-break: lexicographic id)."""
        priv = str(privacy).lower().strip()
        candidates: List[Dict[str, Any]] = [
            d for d in self.devices.values() if str(d.get("status", "active")) == "active"
        ]
        if priv == "high":
            candidates = [d for d in candidates if bool(d.get("capabilities", {}).get("local"))]
        if not candidates:
            return None
        return min(
            candidates,
            key=lambda d: (float(d.get("avg_sigma", 0.5)), str(d.get("id", ""))),
        )

    def sync_calibration(self, source_id: str, target_ids: List[str]) -> None:
        src = self.devices.get(str(source_id))
        if src is None:
            raise KeyError(f"unknown device: {source_id!r}")
        cal = src.get("sigma_calibration")
        for tid in target_ids:
            if tid in self.devices:
                self.devices[tid]["sigma_calibration"] = cal

    def fleet_health(self) -> Dict[str, Any]:
        if not self.devices:
            return {
                "total_devices": 0,
                "active": 0,
                "avg_sigma": 0.0,
                "worst_device": None,
            }
        active = [d for d in self.devices.values() if str(d.get("status", "active")) == "active"]
        avgs = [float(d.get("avg_sigma", 0.5)) for d in self.devices.values()]
        worst = max(self.devices.items(), key=lambda x: float(x[1].get("avg_sigma", 0.5)))[0]
        return {
            "total_devices": len(self.devices),
            "active": len(active),
            "avg_sigma": sum(avgs) / float(len(avgs)),
            "worst_device": worst,
        }


__all__ = ["SigmaFleet"]

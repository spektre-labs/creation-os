# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Kernel-shaped lock metadata for ``cos resolve`` (lab; no CBS search)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import hashlib
import json
from typing import Any, Dict, Sequence, Tuple


class KernelLock:
    """Record a σ-resolution envelope without mutating C kernel bytes (audit stub)."""

    INVARIANTS: Tuple[str, ...] = ("sigma_resolution_only", "no_cbs_search")

    def lock(self, resolution: Dict[str, Any], raw_results: Sequence[Dict[str, Any]]) -> Dict[str, Any]:
        payload = json.dumps({"resolution": resolution, "n": len(raw_results)}, sort_keys=True, default=str)
        digest = hashlib.sha256(payload.encode("utf-8")).hexdigest()[:16]
        return {"status": "locked", "winner": resolution.get("winner"), "envelope_sha16": digest}

    @staticmethod
    def compute_kernel_hash() -> str:
        return "lab_kernel_lock_v1"


__all__ = ["KernelLock"]

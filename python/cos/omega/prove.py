# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""PROVE: cryptographic receipt scaffold (SHA-256, no private key signing here)."""
from __future__ import annotations

import hashlib
import json
import time
from dataclasses import dataclass
from typing import Any, Dict

from cos.sigma_gate_core import sigma_gate

from .state import OmegaContext


@dataclass
class OmegaProve:
    license_tag: str = "LicenseRef-SCSL-1.0 OR AGPL-3.0-only"

    def __call__(self, ctx: OmegaContext, action: Any, result: Any, master: Any) -> Dict[str, Any]:
        payload = {
            "action_sha256": hashlib.sha256(repr(action).encode("utf-8")).hexdigest(),
            "result_sha256": hashlib.sha256(repr(result).encode("utf-8")).hexdigest(),
            "goal_sha256": hashlib.sha256(ctx.goal.encode("utf-8")).hexdigest(),
            "license": self.license_tag,
            "t_ms": int(time.time() * 1000),
            "master_verdict": int(sigma_gate(master)) if hasattr(master, "sigma") else -1,
        }
        canonical = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
        payload["receipt_sha256"] = hashlib.sha256(canonical).hexdigest()
        return payload

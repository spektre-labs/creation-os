# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
σ **signal cascade** labels (L1–L5) for documentation and Python observability.

Ordering is **informational** for lab dashboards; the C cognitive interrupt header
(``sigma_gate.h``) remains the authority for embedded gate math — unchanged by v121.
"""
from __future__ import annotations

from typing import Any, Dict, List

CASCADE_LEVELS: List[Dict[str, Any]] = [
    {"id": "L1", "name": "Entropy", "note": "Cheap uncertainty prior."},
    {"id": "L2", "name": "HIDE", "note": "Hidden-state coherence / contradiction (training-free hints)."},
    {"id": "L3", "name": "ICR", "note": "Inter-column / cross-layer agreement probes."},
    {"id": "L4", "name": "LSD", "note": "Learned distortion / hallucination probe when trained."},
    {
        "id": "L5",
        "name": "SAE",
        "note": "Sparse dictionary: which feature, why, steer vs prune (see sigma_gate_sae_signal).",
    },
]


def cascade_summary() -> Dict[str, Any]:
    """Return ordered cascade metadata for CLI / JSON dashboards."""
    return {"cascade": list(CASCADE_LEVELS), "version": "v121-lab"}


__all__ = ["CASCADE_LEVELS", "cascade_summary"]

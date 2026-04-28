# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
A2A **Agent Card** helpers with a ``sigma_profile`` extension for σ-gate verifier agents.

The Google / Linux Foundation A2A surface advertises capabilities in JSON.  Creation OS
adds structured hallucination-interrupt metadata (signals, verdict vocabulary, gate
version, fixed kernel size) so peers can route verification requests without inventing
ad-hoc fields.

**Measured fields** (for example AUROC) are **optional** and must be supplied by the
operator from a repro bundle; this module does not embed headline benchmark numbers.
See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import os
from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple

# Verifier-oriented capabilities (hallucination / uncertainty), distinct from v250 task caps.
DEFAULT_SIGMA_VERIFIER_CAPABILITIES: Tuple[str, ...] = (
    "hallucination_detection",
    "confidence_scoring",
    "abstention_recommendation",
)

DEFAULT_SIGMA_SIGNALS: Tuple[str, ...] = ("lsd", "hide", "spectral", "entropy")

VERDICT_TYPE_NAMES: Tuple[str, ...] = ("ACCEPT", "RETHINK", "ABSTAIN")


def _gate_version_string() -> str:
    return (os.environ.get("SIGMA_MCP_GATE_VERSION") or "0.57").strip()


def build_sigma_verifier_agent_card(
    *,
    agent_id: str,
    name: str,
    description: str,
    endpoint: str,
    capabilities: Optional[Sequence[str]] = None,
    signals: Optional[Sequence[str]] = None,
    protocols: Sequence[str] = ("mcp", "a2a"),
    license_spdx: str = "LicenseRef-SCSL-1.0 OR AGPL-3.0-only",
    measured_auroc: Optional[float] = None,
    measured_calibration_mean: Optional[float] = None,
    gate_version: Optional[str] = None,
    kernel_size_bytes: int = 12,
    extra_top_level: Optional[Mapping[str, Any]] = None,
) -> Dict[str, Any]:
    """
    Build a JSON-serialisable Agent Card dict including an extended ``sigma_profile``.

    ``kernel_size_bytes`` defaults to ``12`` to match ``sigma_state_t`` in
    ``python/cos/sigma_gate.h`` (fixed-point cognitive interrupt state).

    If ``measured_auroc`` or ``measured_calibration_mean`` is omitted, those keys are
    left out entirely so published cards do not imply unarchived measurements.
    """
    ep = (endpoint or "").strip()
    if not ep:
        raise ValueError("endpoint must be a non-empty URL or path")

    caps: List[str] = list(capabilities or DEFAULT_SIGMA_VERIFIER_CAPABILITIES)
    sigs: List[str] = list(signals or DEFAULT_SIGMA_SIGNALS)

    sigma_profile: Dict[str, Any] = {
        "signals": sigs,
        "verdict_types": list(VERDICT_TYPE_NAMES),
        "gate_version": gate_version if gate_version is not None else _gate_version_string(),
        "kernel_size_bytes": int(kernel_size_bytes),
    }
    if measured_auroc is not None:
        sigma_profile["auroc"] = float(measured_auroc)
    if measured_calibration_mean is not None:
        sigma_profile["calibration_mean"] = float(measured_calibration_mean)

    card: Dict[str, Any] = {
        "agent_id": agent_id,
        "name": name,
        "description": description,
        "capabilities": caps,
        "sigma_profile": sigma_profile,
        "protocols": list(protocols),
        "license": license_spdx,
        "endpoint": ep,
    }
    if extra_top_level:
        for k, v in extra_top_level.items():
            if k not in card:
                card[k] = v
    return card


__all__ = [
    "DEFAULT_SIGMA_SIGNALS",
    "DEFAULT_SIGMA_VERIFIER_CAPABILITIES",
    "VERDICT_TYPE_NAMES",
    "build_sigma_verifier_agent_card",
]

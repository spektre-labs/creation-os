# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Percentile-based **precheck σ** band thresholds for ``SigmaRouter``.

Used when GPT-2 entropy precheck sits in a narrow band (e.g. ~0.35–0.40) so fixed
``tau_fast=0.3`` yields **no** FAST tier — calibrate from an observed holdout list instead.

See ``docs/CLAIM_DISCIPLINE.md`` — savings / tier fractions are harness observables.
"""
from __future__ import annotations

from typing import Dict, Sequence

import numpy as np


def router_thresholds_from_precheck_sigmas(
    sigmas: Sequence[float],
    *,
    p_fast: float = 30.0,
    p_verify: float = 70.0,
    p_escalate: float = 90.0,
) -> Dict[str, float]:
    """
    Return ``tau_fast``, ``tau_verify``, ``tau_escalate`` as percentiles of the sample.

    Policy: ``sigma_pre <= tau_fast`` → FAST band; ``<= tau_verify`` → VERIFY; ``<= tau_escalate``
    → mid band; above → treat as high-risk (router maps to ESCALATE / ABSTAIN depending on
    ``SigmaRouter`` wiring).
    """
    x = np.asarray([float(s) for s in sigmas if s == s], dtype=np.float64)
    if x.size < 3:
        return {"tau_fast": 0.3, "tau_verify": 0.6, "tau_escalate": 0.85}
    return {
        "tau_fast": float(np.percentile(x, p_fast)),
        "tau_verify": float(np.percentile(x, p_verify)),
        "tau_escalate": float(np.percentile(x, p_escalate)),
    }


__all__ = ["router_thresholds_from_precheck_sigmas"]

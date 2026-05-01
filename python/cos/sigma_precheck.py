# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
Shim module: canonical **pre-generation** implementation is ``sigma_gate_precheck``.

Import ``SigmaPrecheck`` from here or from ``cos.sigma_gate_precheck`` interchangeably.
"""
from __future__ import annotations

from .sigma_gate_precheck import SigmaPrecheck

__all__ = ["SigmaPrecheck"]

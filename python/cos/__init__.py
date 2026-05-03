# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Creation OS — σ-aware cognitive architecture (`cos` package; local AI runtime)."""

from __future__ import annotations

from cos.fabric import SigmaFabric
from cos.pipeline import Pipeline, PipelineResult
from cos.sigma_gate import SigmaGate

__version__ = "1.0.0"

__all__ = ["SigmaFabric", "SigmaGate", "Pipeline", "PipelineResult", "__version__"]

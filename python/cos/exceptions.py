# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Shared exceptions for σ-gate surfaces (CLI, decorators, framework hooks)."""


class SigmaAbstainError(RuntimeError):
    """Raised when policy stops the pipeline on an ABSTAIN verdict."""

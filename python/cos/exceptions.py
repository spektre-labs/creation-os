# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Shared exceptions for σ-gate surfaces (CLI, decorators, framework hooks)."""


class SigmaAbstainError(RuntimeError):
    """Raised when policy stops the pipeline on an ABSTAIN verdict."""

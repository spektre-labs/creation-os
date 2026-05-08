# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Backward-compatible ASGI module path for ``uvicorn cos.api.serve:app``.

The canonical implementation lives in :mod:`cos.serve` (REST, WebSocket, SSE).
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from cos.serve import HAS_FASTAPI, create_app

if HAS_FASTAPI:
    app = create_app()
else:  # pragma: no cover — install creation-os[serve] for a concrete ASGI app
    app = None  # type: ignore[assignment]

__all__ = ["app", "create_app"]

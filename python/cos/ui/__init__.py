# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Optional NiceGUI surfaces: σ-dashboard (:mod:`cos.ui.dashboard`) and lab UI (:mod:`cos.ui.app`)."""

from __future__ import annotations

from cos.ui.dashboard import HAS_UI, create_dashboard, run_dashboard

__all__ = ["HAS_UI", "create_dashboard", "run_dashboard"]

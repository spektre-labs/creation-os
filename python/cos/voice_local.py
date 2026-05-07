# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Backward-compatible import path — implementations live in :mod:`cos.voice`."""
from __future__ import annotations

from cos.voice import (
    ABSTAIN_SPOKEN,
    SigmaVoice,
    listen,
    sigma_before_speak,
    speak,
    voice_chat_turn,
)

__all__ = [
    "ABSTAIN_SPOKEN",
    "SigmaVoice",
    "listen",
    "sigma_before_speak",
    "speak",
    "voice_chat_turn",
]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
"""Unit tests for STT confidence → σ (no audio, no optional ASR wheels)."""
from __future__ import annotations

import os
import sys
import unittest
from types import SimpleNamespace

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.abspath(os.path.join(HERE, ".."))
if PKG not in sys.path:
    sys.path.insert(0, PKG)

from cos.sigma_voice_quality import SigmaVoiceQuality  # noqa: E402


class SigmaVoiceQualityTest(unittest.TestCase):
    def test_empty_segments_high_sigma(self) -> None:
        s = SigmaVoiceQuality.stt_sigma_from_segments([])
        self.assertGreaterEqual(s, 0.99)

    def test_confident_logprob_low_sigma(self) -> None:
        segs = [
            SimpleNamespace(avg_logprob=-0.1),
            SimpleNamespace(avg_logprob=-0.06),
        ]
        s = SigmaVoiceQuality.stt_sigma_from_segments(segs)
        self.assertLess(s, 0.2)

    def test_should_retry(self) -> None:
        v = SigmaVoiceQuality(retry_tau=0.5)
        self.assertTrue(v.should_retry(0.51))
        self.assertFalse(v.should_retry(0.49))


if __name__ == "__main__":
    unittest.main()

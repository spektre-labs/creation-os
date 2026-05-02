# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.calibrate``."""
from __future__ import annotations

import os
import tempfile

from cos.calibrate import CalibrationReport, SigmaCalibrator


def test_platt_fit(calibrator) -> None:
    data = [(0.1, 0), (0.2, 0), (0.3, 0), (0.5, 1), (0.7, 1), (0.9, 1)]
    calibrator.fit(data)
    assert calibrator.fitted
    assert calibrator.calibrate(0.9) > calibrator.calibrate(0.1)


def test_isotonic_fit() -> None:
    cal = SigmaCalibrator(method="isotonic")
    data = [(0.1, 0), (0.3, 0), (0.5, 0.5), (0.7, 1), (0.9, 1)]
    cal.fit(data)
    assert cal.fitted
    assert cal.calibrate(0.9) >= cal.calibrate(0.1)


def test_ece(calibrator) -> None:
    data = [(0.1, 0), (0.2, 0), (0.8, 1), (0.9, 1)]
    calibrator.fit(data)
    ece = calibrator.ece(data)
    assert ece is not None
    assert 0 <= ece <= 1


def test_uncalibrated_passthrough() -> None:
    cal = SigmaCalibrator()
    assert cal.calibrate(0.5) == 0.5


def test_save_load(calibrator) -> None:
    data = [(0.1, 0), (0.5, 1), (0.9, 1)]
    calibrator.fit(data)

    path = os.path.join(tempfile.mkdtemp(), "cal.json")
    calibrator.save(path)

    cal2 = SigmaCalibrator()
    cal2.load(path)
    assert cal2.fitted
    assert abs(cal2.calibrate(0.5) - calibrator.calibrate(0.5)) < 0.01


def test_report(calibrator) -> None:
    data = [(0.1, 0), (0.2, 0), (0.5, 1), (0.8, 1), (0.9, 1)]
    calibrator.fit(data)

    report = CalibrationReport(calibrator, data)
    result = report.generate()
    assert "ece" in result
    assert "reliability_diagram" in result

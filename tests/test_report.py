# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.report`."""
from __future__ import annotations

import pytest

from cos.report import SigmaReport


def test_generate_mtier_table_footer() -> None:
    r = SigmaReport()
    m = r.generate_mtier_table({"lab": {"auroc": 0.9, "smece": 0.1, "snr": 1.0, "abstain_pct": "1%"}})
    assert "|" in m and "CLAIM_DISCIPLINE" in m


def test_generate_evidence_ladder() -> None:
    r = SigmaReport()
    txt = r.generate_evidence_ladder({"positives": ["harness JSON"], "negatives": ["demo-only"]})
    assert "falsifiers" in txt.lower() or "Supporting" in txt


def test_generate_reddit_post() -> None:
    r = SigmaReport()
    mt = r.generate_mtier_table({"X": {}})
    ev = r.generate_evidence_ladder({"positives": [], "negatives": []})
    p = r.generate_reddit_post(mt, ev, "https://example.com/repo")
    assert "example.com" in p


def test_format_html() -> None:
    r = SigmaReport()
    h = r.format_body("# hi\n", "html")
    assert "<pre" in h and "hi" in h


def test_format_pdf_raises() -> None:
    with pytest.raises(ValueError, match="PDF"):
        SigmaReport().format_body("x", "pdf")

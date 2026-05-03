# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.webhook`."""
from __future__ import annotations

import hashlib
import hmac
import json

from cos.webhook import SigmaWebhook


def test_on_abstain_registers() -> None:
    w = SigmaWebhook()
    sid = w.on_abstain("https://example.com/h", '{"v":1}')
    assert any(s["id"] == sid for s in w.subscriptions())


def test_signature_hmac() -> None:
    body = json.dumps({"a": 1}, sort_keys=True).encode("utf-8")
    expect = hmac.new(b"secret", body, hashlib.sha256).hexdigest()
    assert SigmaWebhook.signature({"a": 1}, "secret") == expect


def test_retry_policy_shape() -> None:
    p = SigmaWebhook.retry_policy(3, 1.5)
    assert p["max_retries"] == 3


def test_batch_events_groups() -> None:
    w = SigmaWebhook()
    bat = w.batch_events([{"t": 0.0, "x": 1}, {"t": 0.1, "x": 2}, {"t": 10.0, "x": 3}], interval_seconds=0.5)
    assert len(bat) >= 1


def test_delete_subscription() -> None:
    w = SigmaWebhook()
    sid = w.on_incident("https://x.com")
    assert w.delete_subscription(sid) is True
    assert w.delete_subscription(sid) is False

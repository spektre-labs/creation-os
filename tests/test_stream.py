# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for ``cos.stream`` (no live transformers)."""
from __future__ import annotations

from cos.stream import SigmaStream, StreamBuffer, TokenEvent


def test_score_stream(stream) -> None:
    tokens = ["The", "answer", "is", "42"]
    events = list(stream.score_stream("What is the answer?", tokens))
    assert len(events) == 4
    for e in events:
        assert isinstance(e, TokenEvent)
        assert 0 <= e.sigma <= 1


def test_token_event_repr() -> None:
    e = TokenEvent("hello", 0, 0.1, "ACCEPT")
    r = repr(e)
    assert "hello" in r
    assert "ACCEPT" in r


def test_final_event() -> None:
    e = TokenEvent(None, 5, 0.05, "ACCEPT", is_final=True, full_response="hello world")
    assert e.is_final
    assert "FINAL" in repr(e)


def test_repetition_detection(stream) -> None:
    stream = SigmaStream(interrupt_consecutive=2, interrupt_threshold=0.5)
    tokens = ["word"] * 10
    events = list(stream.score_stream("test", tokens))
    high = [e for e in events if e.sigma > 0.2]
    assert len(high) > 0


def test_stream_buffer() -> None:
    buf = StreamBuffer(flush_interval=3)
    e1 = TokenEvent("a", 0, 0.1, "ACCEPT")
    e2 = TokenEvent("b", 1, 0.1, "ACCEPT")

    assert buf.add(e1) is None
    assert buf.add(e2) is None

    e3 = TokenEvent("c", 2, 0.1, "ACCEPT")
    chunk = buf.add(e3)
    assert chunk is not None
    assert len(chunk) == 3


def test_event_to_dict() -> None:
    e = TokenEvent("test", 0, 0.15, "ACCEPT")
    d = e.to_dict()
    assert d["token"] == "test"
    assert d["sigma"] == 0.15

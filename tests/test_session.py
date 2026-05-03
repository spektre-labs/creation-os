# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.session`."""
from __future__ import annotations

from cos.session import SigmaSession


def test_create_and_history() -> None:
    s = SigmaSession()
    c = s.create("user1", "medical")
    sid = c["session_id"]
    s.append_turn(sid, role="user", content="hi", sigma=0.2)
    s.append_turn(sid, role="assistant", content="hello", sigma=0.25)
    h = s.sigma_history(sid)
    assert len(h) == 2


def test_context_window_budget() -> None:
    s = SigmaSession()
    c = s.create("u", None)
    sid = c["session_id"]
    s.append_turn(sid, role="user", content="a" * 100, sigma=0.1)
    win = s.context_window(sid, max_tokens=10)
    assert len(win) >= 1


def test_session_sigma_trend() -> None:
    s = SigmaSession()
    sid = s.create("u", None)["session_id"]
    s.append_turn(sid, role="a", content="x", sigma=0.1)
    s.append_turn(sid, role="a", content="y", sigma=0.5)
    tr = s.session_sigma_trend(sid)
    assert tr.get("rising") is True


def test_multi_turn_and_handoff() -> None:
    s = SigmaSession()
    sid = s.create("u", None)["session_id"]
    s.append_turn(sid, role="u", content="a", sigma=0.2)
    s.append_turn(sid, role="a", content="b", sigma=0.2)
    m = s.multi_turn_sigma(sid)
    assert m["n_sigma_turns"] == 2
    h = s.handoff(sid, "agent_b")
    assert h["ok"] is True


def test_cleanup_old_sessions() -> None:
    s = SigmaSession()
    old_id = s.create("u", None)["session_id"]
    s._sessions[old_id]["created"] = 0.0
    assert s.cleanup(max_age_minutes=1e-6) >= 1

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for :mod:`cos.state`."""
from __future__ import annotations

from cos.state import SigmaState


def test_set_get_ephemeral() -> None:
    st = SigmaState()
    st.set("k", "v", "ephemeral", 0.1)
    r = st.get("k", "ephemeral")
    assert r["value"] == "v" and r["sigma"] == 0.1


def test_session_isolation() -> None:
    st = SigmaState()
    st.set("k", 1, "session", 0.2, session_id="A")
    st.set("k", 2, "session", 0.2, session_id="B")
    assert st.get("k", "session", session_id="A")["value"] == 1
    assert st.get("k", "session", session_id="B")["value"] == 2


def test_persistent_and_stale() -> None:
    st = SigmaState()
    st.set("p", "x", "persistent", 0.05)
    d = st.stale_detection("p", "persistent")
    assert "effective_sigma" in d


def test_merge_prefers_lower_sigma() -> None:
    st = SigmaState()
    sess = {"a": {"value": 1, "sigma": 0.4}}
    pers = {"a": {"value": 2, "sigma": 0.2}}
    m = st.merge(sess, pers, session_id="s")
    assert m["merged"]["a"]["value"] == 2


def test_gc_ephemeral() -> None:
    st = SigmaState()
    st.set("z", 1, "ephemeral", 0.1)
    n = st.gc("ephemeral", max_age_s=-1.0)
    assert n >= 1


def test_get_missing() -> None:
    st = SigmaState()
    assert st.get("nope", "ephemeral")["missing"] is True

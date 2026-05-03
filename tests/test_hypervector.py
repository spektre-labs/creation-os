# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

import pytest

pytest.importorskip("numpy")

from cos.hypervector import HDCodebook, HyperVector, SigmaHDC

_DIM = 256


def test_bind_involution() -> None:
    rng = __import__("numpy").random.default_rng(0)
    a = HyperVector.random(_DIM, rng)
    b = HyperVector.random(_DIM, rng)
    t = HyperVector.bind(a, HyperVector.bind(a, b))
    assert HyperVector.similarity(t, b) == pytest.approx(1.0)


def test_bundle_reduces_to_superposition() -> None:
    rng = __import__("numpy").random.default_rng(1)
    vs = [HyperVector.random(_DIM, rng) for _ in range(5)]
    sup = HyperVector.bundle(vs)
    assert sup.dim == _DIM
    assert HyperVector.similarity(sup, vs[0]) > -1.0


def test_encode_triple_deterministic() -> None:
    lab = SigmaHDC(dim=_DIM, seed=42)
    h1 = lab.encode_triple("Paris", "capital_of", "France")
    h2 = lab.encode_triple("Paris", "capital_of", "France")
    assert HyperVector.similarity(h1, h2) == pytest.approx(1.0)


def test_query_triple_recovers_object_similarity() -> None:
    lab = SigmaHDC(dim=_DIM, seed=7)
    t = lab.encode_triple("X", "rel", "Y")
    q = lab.query_triple(t, "X", "rel")
    y = lab.book["Y"]
    assert HyperVector.similarity(q, HyperVector.permute(y, 1)) == pytest.approx(1.0)


def test_sequence_sigma_non_negative() -> None:
    lab = SigmaHDC(dim=_DIM, seed=3)
    s = lab.sequence_sigma(["a", "b", "c", "d"])
    assert 0.0 <= s <= 1.0


def test_memory_accounting_int8_vs_bitpacked() -> None:
    rng = __import__("numpy").random.default_rng(2)
    v = HyperVector.random(10000, rng)
    assert v.memory_bytes() == 10000
    assert v.bitpacked_bytes() == 1250
    assert len(v.to_bitpacked()) == 1250


def test_bitpacked_roundtrip() -> None:
    rng = __import__("numpy").random.default_rng(99)
    v = HyperVector.random(_DIM, rng)
    w = HyperVector.from_bitpacked(v.to_bitpacked(), dim=_DIM)
    assert HyperVector.similarity(v, w) == pytest.approx(1.0)


def test_permute_changes_vector() -> None:
    rng = __import__("numpy").random.default_rng(5)
    v = HyperVector.random(_DIM, rng)
    w = HyperVector.permute(v, 3)
    assert HyperVector.similarity(v, w) < 1.0


def test_hdcodebook_cache_shared_dim() -> None:
    cb = HDCodebook(_DIM, seed=0)
    a = cb["foo"]
    b = cb["foo"]
    assert a is b


def test_sigma_hdc_uses_gate() -> None:
    lab = SigmaHDC(dim=_DIM, seed=1)
    assert lab.gate is not None
    hv = lab.encode_triple("s", "r", "o")
    assert hv.memory_bytes() == _DIM

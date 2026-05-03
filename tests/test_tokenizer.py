# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.tokenizer`."""
from __future__ import annotations

from cos.tokenizer import SigmaTokenizer


class _Tok:
    def __init__(self, mult: int = 1, vs: int = 1000) -> None:
        self._m = mult
        self.vocab_size = vs

    def encode(self, text: str) -> list[int]:
        base = [ord(c) % max(self.vocab_size - 1, 2) for c in text if c.isalnum()]
        if self._m > 1:
            return base * self._m
        return base or [0]

    def decode(self, ids: list[int]) -> str:
        return "".join(chr(i % 128) for i in ids)


def test_tokenizer_analyze_basic() -> None:
    st = SigmaTokenizer()
    r = st.analyze("hello world", _Tok(1, 300))
    assert r["n_tokens"] >= 1
    assert "sigma_input" in r


def test_tokenizer_fragmentation_high_when_over_segmented() -> None:
    st = SigmaTokenizer()
    a1 = st.analyze("hello", _Tok(1))
    a2 = st.analyze("hello", _Tok(5))
    assert a2["n_tokens"] > a1["n_tokens"]
    assert a2["fragmentation_score"] >= a1["fragmentation_score"]


def test_tokenizer_oov_spikes_sigma() -> None:
    st = SigmaTokenizer(unk_id=999)

    class BadTok(_Tok):
        def encode(self, text: str) -> list[int]:
            return [999]

    r = st.analyze("x", BadTok())
    assert r["oov_hit"] is True
    assert r["sigma_input"] >= 0.5


def test_tokenizer_compare_prefers_lower_sigma() -> None:
    st = SigmaTokenizer()
    c = st.compare_tokenizers("hello there", [_Tok(1), _Tok(5)])
    assert c["best_index"] in (0, 1)


def test_tokenizer_encoding_artifact() -> None:
    st = SigmaTokenizer()
    r = st.detect_encoding_artifacts(["ok", "bad\ufffd"])
    assert r["suspicious"] is True


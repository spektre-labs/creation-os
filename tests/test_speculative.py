# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

from __future__ import annotations

from cos.sigma_gate import SigmaGate
from cos.speculative import SigmaSpeculative


def test_draft_mock_model() -> None:
    spec = SigmaSpeculative()

    def draft_model(_p: str, n: int) -> list[str]:
        return [f"w{i}" for i in range(n)]

    out = spec.draft("hello", draft_model, 4)
    assert len(out) == 4
    assert out[0] == "w0"


def test_verify_accepts_prefix() -> None:
    spec = SigmaSpeculative()
    gate = SigmaGate(threshold_accept=0.26, threshold_abstain=0.85)
    vr = spec.verify("What is 2+2?", ["4"], gate)
    assert vr["rejected_at"] is None or len(vr["accepted"]) >= 1


def test_verify_rejects_eventually() -> None:
    spec = SigmaSpeculative()
    gate = SigmaGate()
    toks = ["Z9µ"] * 12
    vr = spec.verify("calm prompt", toks, gate)
    assert vr["rejected_at"] is not None or len(vr["accepted"]) < len(toks)


def test_draft_length_increases_when_sigma_low() -> None:
    spec = SigmaSpeculative(min_draft=1, max_draft=8)
    hi = spec.draft_length_from_sigma(0.95)
    lo = spec.draft_length_from_sigma(0.05)
    assert lo >= hi


def test_speculative_generate_all_draft() -> None:
    spec = SigmaSpeculative()

    def draft_fn(_p: str, n: int) -> list[str]:
        return ["a", "b"][: max(1, min(n, 2))]

    def target_fn(_c: str, _n: int) -> str:
        return "fallback"

    gate = SigmaGate()
    out = spec.speculative_generate("p", draft_fn, target_fn, gate)
    assert out["text"]
    assert out["mode"] in ("draft_only", "draft_plus_target")


def test_speculative_generate_fallback_target() -> None:
    spec = SigmaSpeculative()

    def bad_draft(_p: str, n: int) -> list[str]:
        return ["###"] * n

    def target_fn(_c: str, _n: int) -> str:
        return "recovered"

    gate = SigmaGate()
    out = spec.speculative_generate("explain tensors", bad_draft, target_fn, gate)
    assert out["text"]
    if out["mode"] == "draft_plus_target":
        assert "recovered" in out["text"]


def test_tree_draft_structure() -> None:
    spec = SigmaSpeculative()

    def dm(p: str, n: int) -> list[str]:
        base = abs(hash(p)) % 97
        return [f"t{base}_{i}" for i in range(n)]

    tree = spec.tree_draft("root", dm, branches=2, depth=2)
    assert isinstance(tree["root"], list)
    assert len(tree["children"]) == 2


def test_deterministic_verify() -> None:
    spec = SigmaSpeculative()
    gate = SigmaGate()
    toks = ["alpha", "beta"]
    a = spec.verify("ctx", toks, gate)
    b = spec.verify("ctx", toks, gate)
    assert a["accepted"] == b["accepted"]
    assert a["rejected_at"] == b["rejected_at"]


class _GateVerdict:
    def __init__(self, verdict: str, sigma: float = 0.5) -> None:
        self._verdict = verdict
        self._sigma = sigma

    def score(self, _prompt: str, _response: str) -> tuple[float, str]:
        return self._sigma, self._verdict


def test_decode_produces_tokens() -> None:
    spec = SigmaSpeculative(gate=_GateVerdict("ACCEPT", 0.1))
    n = {"i": 0}

    def draft_fn(_p: str, k: int) -> list[str]:
        k = min(k, 3)
        out = []
        for _ in range(k):
            out.append(f"w{n['i']}")
            n["i"] += 1
        return out

    def verify_fn(_p: str, _t: str | None) -> str | None:
        return "BAD"

    out = spec.decode("start", draft_fn, verify_fn, max_tokens=5, k=3)
    assert len(out["tokens"]) == 5
    assert out["text"].startswith("w")


def test_low_sigma_skips_verification() -> None:
    calls: list[str | None] = []
    spec = SigmaSpeculative(gate=_GateVerdict("ACCEPT", 0.05))

    def draft_fn(_p: str, k: int) -> list[str]:
        return ["a", "b", "c"][:k]

    def verify_fn(_p: str, tok: str | None) -> str | None:
        calls.append(tok)
        return "v"

    spec.decode("p", draft_fn, verify_fn, max_tokens=4, k=3)
    assert calls == []


def test_high_sigma_uses_target() -> None:
    calls: list[str | None] = []
    spec = SigmaSpeculative(gate=_GateVerdict("ABSTAIN", 0.95))

    def draft_fn(_p: str, _k: int) -> list[str]:
        return ["draft"]

    def verify_fn(_p: str, tok: str | None) -> str | None:
        calls.append(tok)
        return "from_target" if tok is None else None

    out = spec.decode("p", draft_fn, verify_fn, max_tokens=3, k=2)
    assert None in calls
    assert "from_target" in out["text"]


def test_efficiency_stats() -> None:
    spec = SigmaSpeculative(gate=_GateVerdict("ACCEPT", 0.1))

    def draft_fn(_p: str, k: int) -> list[str]:
        return ["t1", "t2"][:k]

    def verify_fn(_p: str, _t: str | None) -> str | None:
        return None

    out = spec.decode("p", draft_fn, verify_fn, max_tokens=2, k=2)
    stats = out["stats"]
    for key in ("draft_accept_rate", "verify_rate", "reject_rate", "speedup_estimate", "tokens_generated"):
        assert key in stats


def test_speedup_estimate() -> None:
    spec = SigmaSpeculative(gate=_GateVerdict("ACCEPT", 0.02))

    def draft_fn(_p: str, k: int) -> list[str]:
        return [f"x{i}" for i in range(k)]

    def verify_fn(_p: str, _t: str | None) -> str | None:
        raise RuntimeError("verify should not run")

    out = spec.decode("p", draft_fn, verify_fn, max_tokens=8, k=4)
    assert out["stats"]["speedup_estimate"] > 1.0
    assert out["stats"]["draft_accept_rate"] >= 0.99


def test_empty_draft_stops() -> None:
    spec = SigmaSpeculative(gate=_GateVerdict("ACCEPT", 0.1))

    def draft_fn(_p: str, _k: int) -> list[str]:
        return []

    def verify_fn(_p: str, _t: str | None) -> str | None:
        return "no"

    out = spec.decode("p", draft_fn, verify_fn, max_tokens=5, k=3)
    assert out["tokens"] == []
    assert out["text"] == ""

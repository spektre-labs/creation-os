# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
from __future__ import annotations

import sys
from typing import Any
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate_core import Verdict  # noqa: E402
from cos.sigma_sense import SigmaSense  # noqa: E402
from cos.sigma_sim2real import SigmaSim2Real  # noqa: E402
from cos.sigma_vla import SigmaVLA  # noqa: E402


def test_sigma_sense_pessimistic_max() -> None:
    s = SigmaSense(
        vision_sigma=lambda _i: 0.2,
        text_sigma=lambda t: 0.9 if "bad" in str(t) else 0.1,
    )
    comb, m = s.perceive({"image": b"x", "text": "bad caption"})
    assert m["vision"] == 0.2 and m["text"] == 0.9
    assert "cross_modal" in m
    assert comb == max(m.values())


def test_sigma_vla_accept_and_abstain() -> None:
    sense = SigmaSense(
        vision_sigma=lambda _x: 0.0,
        text_sigma=lambda _t: 0.0,
        cross_modal_sigma=lambda _inp, _sig: 0.0,
    )

    def policy(_o: object, _i: str) -> str:
        return "noop"

    def act_sig(_a: object) -> float:
        return 0.0

    def human(_o: object, _i: str) -> str:
        return "human_override"

    vla = SigmaVLA(
        sense=sense,
        policy=policy,
        compute_action_sigma=act_sig,
        request_human_input=human,
        k_raw=0.95,
    )
    a, v = vla.act(b"img", "go")
    assert v == Verdict.ACCEPT and a == "noop"

    sense2 = SigmaSense(
        vision_sigma=lambda _x: 0.25,
        text_sigma=lambda _t: 0.25,
        cross_modal_sigma=lambda _inp, _sig: 0.0,
    )

    def policy2(*_a: object) -> str:
        return "move"

    def act_sig2(_a: object) -> float:
        return 0.25

    vla2 = SigmaVLA(
        sense=sense2,
        policy=policy2,
        compute_action_sigma=act_sig2,
        request_human_input=human,
        k_raw=0.95,
    )
    a2, v2 = vla2.act(b"img", "go")
    assert v2 == Verdict.RETHINK and a2 == "human_override"

    sense3 = SigmaSense(
        vision_sigma=lambda _x: 0.95,
        text_sigma=lambda _t: 0.95,
        cross_modal_sigma=lambda _inp, _sig: 0.0,
    )

    vla3 = SigmaVLA(
        sense=sense3,
        policy=policy2,
        compute_action_sigma=lambda _a: 0.95,
        request_human_input=human,
        k_raw=0.95,
    )
    a3, v3 = vla3.act(b"img", "go")
    assert v3 == Verdict.ABSTAIN and a3 is None


def test_sigma_sim2real_track_and_adapt() -> None:
    s2r = SigmaSim2Real(threshold=0.2)
    v, g = s2r.track_gap(0.3, 0.7)
    assert v == Verdict.RETHINK and abs(g - 0.4) < 1e-9
    v2, g2 = s2r.track_gap(0.4, 0.5)
    assert v2 == Verdict.ACCEPT

    steps: list[tuple[Any, ...]] = []

    def ttt(m: Any, obs: Any, out: Any) -> None:
        steps.append((m, obs, out))

    s2r.ttt_update = ttt
    s2r.compute_sigma = lambda out: 0.9  # noqa: E731
    s2r.get_sim_baseline = lambda obs: 0.2  # noqa: E731

    class M:
        def __call__(self, obs: object) -> str:
            return f"out-{obs}"

    n = s2r.adapt(M(), [1, 2, 3])
    assert n == 3 and len(steps) == 3

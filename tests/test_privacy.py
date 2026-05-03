# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Tests for :mod:`cos.privacy`."""
from __future__ import annotations

import math

from cos import SigmaGate
from cos.privacy import SigmaPrivacy


def test_privacy_gradient_clip_reduces_norm() -> None:
    gate = SigmaGate()
    priv = SigmaPrivacy(rng=__import__("random").Random(0))
    g = {"a": [3.0, 4.0]}
    clipped = priv.gradient_clip(g, max_norm=1.0, gate=gate)
    n = math.sqrt(sum(x * x for x in clipped["a"]))
    assert n <= 1.0001


def test_privacy_dp_noise_changes_vector() -> None:
    gate = SigmaGate()
    priv = SigmaPrivacy(rng=__import__("random").Random(1))
    g = {"l": [0.5, -0.25, 0.1]}
    n = priv.dp_noise(g, epsilon=2.0, gate=gate)
    assert n["l"] != g["l"]


def test_privacy_budget_fields() -> None:
    b = SigmaPrivacy.privacy_budget(1.0, 1e-5, 10)
    assert "approx_remaining" in b
    assert b["steps"] == 10


def test_privacy_pii_redact() -> None:
    priv = SigmaPrivacy()
    t = "Contact foo@example.com or +1 650-555-0199"
    r = priv.pii_redact(t)
    assert "foo@example.com" not in r
    assert "PHONE_REDACTED" in r or "EMAIL_REDACTED" in r


def test_privacy_audit_trail() -> None:
    priv = SigmaPrivacy(rng=__import__("random").Random(2))
    gate = SigmaGate()
    priv.dp_noise({"x": [1.0, 2.0]}, epsilon=3.0, gate=gate, layer_sigma_override={"x": 0.1})
    aud = priv.audit_trail()
    assert any(x.get("op") == "dp_noise" for x in aud)


def test_privacy_layer_override_high_sigma_less_perturbation() -> None:
    gate = SigmaGate()
    g0 = {"a": [0.0, 0.0, 0.0]}
    lo = SigmaPrivacy(rng=__import__("random").Random(3)).dp_noise(
        g0, 1.0, gate, layer_sigma_override={"a": 0.05}
    )
    hi = SigmaPrivacy(rng=__import__("random").Random(3)).dp_noise(
        g0, 1.0, gate, layer_sigma_override={"a": 0.95}
    )
    assert sum(abs(x) for x in lo["a"]) >= sum(abs(x) for x in hi["a"]) - 1e-9


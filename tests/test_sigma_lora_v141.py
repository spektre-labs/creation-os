# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v141 σ-LoRA: adapter registry, σ-select, merge threshold, synthetic train sweep."""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate_core import Verdict
from cos.sigma_lora import SigmaLoRA


class _ToyBase:
    def __init__(self) -> None:
        self._active: Optional[str] = None

    def load_adapter(self, path: str, **kwargs: Any) -> None:
        name = kwargs.get("name")
        self._active = str(name) if name is not None else path.split("/")[-1]

    def unload_adapter(self, name: Optional[str] = None) -> None:
        _ = name
        self._active = None

    def merge_and_unload(self, name: str) -> None:
        _ = name
        self._active = None

    def generate(self, prompt: str) -> str:
        _ = prompt
        a = (self._active or "").lower()
        if "bad_merge" in a:
            return "UNSAFE_MERGE_MARKER speculative nonsense"
        if a == "medical":
            return "Aspirin may cause GI bleeding; consult a clinician."
        if a == "legal":
            return "Hypertension documentation may fall under employment-disability rules."
        if a == "code":
            return "def hypertension_risk(x): return x > 140"
        return "General wellness advice."


class _SelectGate:
    def score(self, prompt: str, response: str) -> tuple[float, Verdict]:
        _ = prompt
        r = response.lower()
        if "clinician" in r or "gi bleeding" in r:
            return 0.08, Verdict.ACCEPT
        if "employment" in r:
            return 0.31, Verdict.RETHINK
        if "hypertension_risk" in r:
            return 0.18, Verdict.ACCEPT
        return 0.45, Verdict.RETHINK


def test_three_adapters_auto_select_lowest_sigma() -> None:
    mgr = SigmaLoRA(_ToyBase(), _SelectGate())
    mgr.register("medical", "adapters/medical", domain="medicine")
    mgr.register("legal", "adapters/legal", domain="law")
    mgr.register("code", "adapters/code", domain="programming")
    pick = mgr.select_adapter("What are the side effects of aspirin?")
    assert pick == "medical"
    row = mgr.inference_with_adapter("What are the side effects of aspirin?", adapter_name=pick)
    assert row["adapter"] == "medical"
    assert float(row["sigma"]) < 0.2


class _MergeGate:
    def score(self, prompt: str, response: str) -> tuple[float, Verdict]:
        _ = prompt
        if "UNSAFE_MERGE_MARKER" in response:
            return 0.85, Verdict.ABSTAIN
        return 0.1, Verdict.ACCEPT


def test_merge_blocked_when_sigma_above_threshold() -> None:
    mgr = SigmaLoRA(_ToyBase(), _MergeGate(), merge_sigma_max=0.3)
    mgr.register("bad_merge", "adapters/bad_merge")
    out = mgr.merge_adapter("bad_merge", validation_prompts=["Explain hypertension briefly."])
    assert out["merged"] is False
    assert float(out["sigma"]) > 0.3


def test_train_synthetic_early_stop_high_sigma_epoch() -> None:
    mgr = SigmaLoRA(_ToyBase(), _SelectGate())
    out = mgr.train_adapter_synthetic(
        "customer_support",
        adapter_path="adapters/customer_support",
        val_sigma_by_epoch=[0.2, 0.22, 0.55, 0.6],
        epochs=5,
        sigma_stop=0.5,
        domain="support",
    )
    assert out.get("early_stop") is True
    assert int(out.get("stopped_epoch", -1)) == 2

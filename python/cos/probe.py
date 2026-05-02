# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-probe: hidden-state extraction + multi-signal cascade (L1–L4 lab).

**L1** (entropy proxy): always available, pure Python.

**L2–L4** need layer hidden states (and **L4** logits). Use ``pip install 'creation-os[probes]'``
and a HuggingFace-style ``model`` with ``output_hidden_states=True`` forward, or hook-based capture.

This module does **not** ship a trained SAE; **L5** remains a future hook.
"""
from __future__ import annotations

import contextlib
import math
from typing import Any, Callable, Dict, List, Optional

# --- SigmaProbe -----------------------------------------------------------------


class SigmaProbe:
    """Extract hidden states + logits/attention from a forward pass."""

    def __init__(self, model: Any = None, method: str = "auto") -> None:
        self.model = model
        self.method = str(method)
        self.hooks: List[Any] = []
        self.captured_states: Dict[str, Any] = {}

    def extract(
        self,
        input_ids: Any,
        attention_mask: Optional[Any] = None,
    ) -> Dict[str, Any]:
        if self.model is None:
            return {"hidden_states": None, "logits": None, "attentions": None}

        if self.method == "auto":
            self.method = self._detect_method()

        if self.method == "huggingface":
            return self._extract_hf(input_ids, attention_mask)
        if self.method == "hook":
            return self._extract_hook(input_ids, attention_mask)
        return {"hidden_states": None, "logits": None, "attentions": None}

    def _detect_method(self) -> str:
        if hasattr(self.model, "config") and callable(getattr(self.model, "forward", None)):
            return "huggingface"
        return "hook"

    def _extract_hf(
        self,
        input_ids: Any,
        attention_mask: Optional[Any],
    ) -> Dict[str, Any]:
        try:
            import torch
        except ImportError as e:  # pragma: no cover
            raise ImportError("HuggingFace extract requires PyTorch. Install: creation-os[probes]") from e

        with torch.no_grad():
            outputs = self.model(
                input_ids,
                attention_mask=attention_mask,
                output_hidden_states=True,
                output_attentions=True,
                return_dict=True,
            )
        return {
            "hidden_states": getattr(outputs, "hidden_states", None),
            "logits": getattr(outputs, "logits", None),
            "attentions": getattr(outputs, "attentions", None),
        }

    def _extract_hook(
        self,
        input_ids: Any,
        attention_mask: Optional[Any],
    ) -> Dict[str, Any]:
        self.captured_states = {}
        handles: List[Any] = []

        for name, module in self.model.named_modules():
            if self._is_layer(name, module):
                handles.append(module.register_forward_hook(self._make_hook(name)))

        with self._no_grad():
            if attention_mask is not None:
                output = self.model(input_ids, attention_mask=attention_mask)
            else:
                output = self.model(input_ids)

        for h in handles:
            h.remove()

        logits = None
        if hasattr(output, "logits"):
            logits = output.logits
        elif hasattr(output, "shape"):
            logits = output

        return {
            "hidden_states": list(self.captured_states.values()) or None,
            "logits": logits,
            "attentions": None,
        }

    def _make_hook(self, name: str) -> Callable[..., None]:
        def hook(_module: Any, _inp: Any, output: Any) -> None:
            if hasattr(output, "detach"):
                self.captured_states[name] = output.detach()
            elif isinstance(output, tuple) and len(output) > 0 and hasattr(output[0], "detach"):
                self.captured_states[name] = output[0].detach()

        return hook

    @staticmethod
    def _is_layer(name: str, module: Any) -> bool:
        del module
        layer_patterns = ("layers.", "h.", "blocks.", "decoder.layer")
        return any(p in name for p in layer_patterns)

    @staticmethod
    def _no_grad() -> contextlib.AbstractContextManager[Any]:
        try:
            import torch

            return torch.no_grad()
        except ImportError:
            return contextlib.nullcontext()


# --- SignalCascade --------------------------------------------------------------


class SignalCascade:
    """Cheapest-first σ signals: L1 always; L2–L4 when tensors are supplied."""

    def __init__(
        self,
        _probe: Optional[SigmaProbe] = None,
        thresholds: Optional[Dict[str, float]] = None,
    ) -> None:
        _ = _probe
        t = thresholds or {}
        self.thresholds = {
            "early_accept": float(t.get("early_accept", 0.1)),
            "early_abstain": float(t.get("early_abstain", 0.9)),
            "tau_accept": float(t.get("tau_accept", 0.3)),
            "tau_abstain": float(t.get("tau_abstain", 0.8)),
        }

    def score(
        self,
        prompt: str,
        response: str,
        hidden_states: Any = None,
        logits: Any = None,
    ) -> Dict[str, Any]:
        del prompt
        signals: Dict[str, float] = {}
        signals["L1_entropy"] = float(self._l1_entropy(response or ""))
        sigma = signals["L1_entropy"]
        level = 1

        if sigma < self.thresholds["early_accept"]:
            return self._result(sigma, signals, level)
        if sigma > self.thresholds["early_abstain"]:
            return self._result(sigma, signals, level)

        if hidden_states is None:
            return self._result(sigma, signals, level)

        hs = self._as_layer_list(hidden_states)
        if len(hs) < 2:
            return self._result(sigma, signals, level)

        signals["L2_hide"] = float(self._l2_hide(hs))
        sigma = 0.5 * signals["L1_entropy"] + 0.5 * signals["L2_hide"]
        level = 2
        if sigma < self.thresholds["early_accept"]:
            return self._result(sigma, signals, level)
        if sigma > self.thresholds["early_abstain"]:
            return self._result(sigma, signals, level)

        signals["L3_icr"] = float(self._l3_icr(hs))
        sigma = (
            0.35 * signals["L1_entropy"]
            + 0.35 * signals["L2_hide"]
            + 0.30 * signals["L3_icr"]
        )
        level = 3
        if sigma < self.thresholds["early_accept"]:
            return self._result(sigma, signals, level)
        if sigma > self.thresholds["early_abstain"]:
            return self._result(sigma, signals, level)

        if logits is not None:
            l4 = self._l4_lsd(logits)
            if l4 is not None:
                signals["L4_lsd"] = float(l4)
                sigma = (
                    0.25 * signals["L1_entropy"]
                    + 0.25 * signals["L2_hide"]
                    + 0.25 * signals["L3_icr"]
                    + 0.25 * signals["L4_lsd"]
                )
                level = 4

        return self._result(sigma, signals, level)

    @staticmethod
    def _as_layer_list(hidden_states: Any) -> List[Any]:
        if hidden_states is None:
            return []
        if isinstance(hidden_states, (list, tuple)):
            return list(hidden_states)
        return [hidden_states]

    def _l1_entropy(self, text: str) -> float:
        if not text:
            return 1.0
        freq: Dict[str, int] = {}
        for c in text.lower():
            freq[c] = freq.get(c, 0) + 1
        total = len(text)
        entropy = -sum(
            (count / total) * math.log2(count / total) for count in freq.values() if count > 0
        )
        return max(0.0, min(1.0, 1.0 - entropy / 4.7))

    def _l2_hide(self, hidden_states: List[Any]) -> float:
        if len(hidden_states) < 2:
            return 0.5
        last = hidden_states[-1]
        prev = hidden_states[-2]
        if hasattr(last, "float") and hasattr(prev, "float"):
            try:
                diff = float((last.float() - prev.float()).norm().item())
                norm = float(last.float().norm().item()) + 1e-8
                return min(1.0, diff / norm)
            except Exception:
                return 0.5
        flat_last = self._flatten(last)
        flat_prev = self._flatten(prev)
        if not flat_last or not flat_prev:
            return 0.5
        diff = sum((a - b) ** 2 for a, b in zip(flat_last, flat_prev)) ** 0.5
        norm = sum(a * a for a in flat_last) ** 0.5 + 1e-8
        return min(1.0, diff / norm)

    def _l3_icr(self, hidden_states: List[Any]) -> float:
        if len(hidden_states) < 3:
            return 0.5
        drifts: List[float] = []
        for i in range(1, min(len(hidden_states), 6)):
            d = self._l2_hide([hidden_states[i - 1], hidden_states[i]])
            drifts.append(float(d))
        if not drifts:
            return 0.5
        mean_drift = sum(drifts) / len(drifts)
        variance = sum((d - mean_drift) ** 2 for d in drifts) / len(drifts)
        return min(1.0, variance * 10.0)

    def _l4_lsd(self, logits: Any) -> Optional[float]:
        if logits is None:
            return None
        if not hasattr(logits, "float"):
            return None
        try:
            last_logits = logits[0, -1, :].float()
            probs = last_logits.softmax(dim=-1)
            entropy = float(-(probs * probs.log().clamp(min=-100)).sum().item())
            max_entropy = math.log(max(int(probs.shape[0]), 2))
            if max_entropy <= 0:
                return 0.5
            return min(1.0, entropy / max_entropy)
        except Exception:
            return None

    def _flatten(self, x: Any) -> List[float]:
        if isinstance(x, (int, float)):
            return [float(x)]
        if isinstance(x, list):
            out: List[float] = []
            for item in x:
                out.extend(self._flatten(item))
            return out
        return []

    def _result(self, sigma: float, signals: Dict[str, float], level: int) -> Dict[str, Any]:
        if sigma < self.thresholds["tau_accept"]:
            verdict = "ACCEPT"
        elif sigma > self.thresholds["tau_abstain"]:
            verdict = "ABSTAIN"
        else:
            verdict = "RETHINK"
        return {
            "sigma": float(sigma),
            "verdict": verdict,
            "levels": signals,
            "cascade_depth": int(level),
            "cost_saved": f"stopped at L{level}/5",
        }


__all__ = ["SigmaProbe", "SignalCascade"]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-probe: hidden-state extraction + multi-signal cascade (L1–L4 lab).

**L1** (entropy proxy): always available, pure Python.

**L2–L4** need layer hidden states (and **L4** logits). Use ``pip install 'creation-os[probes]'``
and a HuggingFace-style ``model`` with ``output_hidden_states=True`` forward, or hook-based capture.

**L5** hooks (spectral / semantic-entropy / energy surrogates) complement **L1–L4**; **L6**
(:mod:`cos.sink_probe` / :func:`cos.cascade.cascade_L6`) uses attention maps when wired. See
:class:`ICRProbe`, :class:`SEPProbe`, :class:`SpectralProbe`, :class:`EnergyProbe`.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import contextlib
import math
from typing import Any, Callable, Dict, List, Optional

from cos.config import DEFAULT_CONFIG


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
            raise ImportError(
                "HuggingFace extract requires PyTorch. Install: creation-os[probes]",
            ) from e

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
        t: Dict[str, float] = dict(thresholds) if thresholds else {}
        self.thresholds = {
            "early_accept": float(t.get("early_accept", 0.1)),
            "early_abstain": float(t.get("early_abstain", 0.9)),
            "threshold_accept": float(
                t.get("threshold_accept", t.get("tau_accept", DEFAULT_CONFIG.threshold_accept))
            ),
            "threshold_abstain": float(
                t.get("threshold_abstain", t.get("tau_abstain", DEFAULT_CONFIG.threshold_abstain))
            ),
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

    @staticmethod
    def as_layer_list(hidden_states: Any) -> List[Any]:
        """Public alias for cascade callers."""
        return SignalCascade._as_layer_list(hidden_states)

    def hide_score(self, hidden_states: List[Any]) -> float:
        """L2 consecutive-layer drift in ``[0, 1]``."""
        return float(self._l2_hide(hidden_states))

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
        if sigma < self.thresholds["threshold_accept"]:
            verdict = "ACCEPT"
        elif sigma < self.thresholds["threshold_abstain"]:
            verdict = "RETHINK"
        else:
            verdict = "ABSTAIN"
        return {
            "sigma": float(sigma),
            "verdict": verdict,
            "levels": signals,
            "cascade_depth": int(level),
            "cost_saved": f"stopped at L{level}/5",
        }


def _feature_vec(x: Any, *, max_dim: int = 64) -> List[float]:
    """Mean-pool tensors to a fixed small vector for lab probes (NumPy-free)."""
    sc = SignalCascade()
    flat = sc._flatten(x)[:8192]
    if not flat:
        return [0.0] * min(8, max_dim)
    chunk = max(1, len(flat) // max_dim)
    out: List[float] = []
    for i in range(max_dim):
        s = i * chunk
        e = min(len(flat), s + chunk)
        if s >= e:
            out.append(0.0)
        else:
            out.append(sum(flat[s:e]) / (e - s))
    return out[:max_dim]


class ICRProbe:
    """Hidden-state **update** signal: variance of consecutive-layer deltas (Orgad-style caveat:
    no single-layer probe; cross-layer path only). Training-free on states."""

    def score(self, hidden_states: Any) -> float:
        hs = SignalCascade.as_layer_list(hidden_states)
        if len(hs) < 2:
            return 0.5
        sc = SignalCascade()
        deltas: List[float] = []
        for i in range(1, len(hs)):
            d = float(sc.hide_score([hs[i - 1], hs[i]]))
            deltas.append(d)
        mean_d = sum(deltas) / len(deltas)
        var = sum((d - mean_d) ** 2 for d in deltas) / len(deltas)
        return float(max(0.0, min(1.0, var * 12.0)))


class EnergyProbe:
    """Training-free energy-style stress: hidden norm curvature (lite surrogate, no EBMs)."""

    @staticmethod
    def score_hidden_tensor(h: Any) -> float:
        v = _feature_vec(h, max_dim=32)
        n = math.sqrt(sum(x * x for x in v)) / (len(v) ** 0.5 or 1.0)
        return float(max(0.0, min(1.0, 1.0 - math.exp(-n))))


class SEPProbe:
    """Linear semantic-entropy probe (lab): fit ``w·h → pseudo H_sem`` from repeated samples."""

    def __init__(self) -> None:
        self.dim = 12
        self.weight: List[float] = [1.0 / 12.0] * 12
        self.bias: float = 0.0
        self.fitted: bool = False

    @staticmethod
    def pseudo_semantic_entropy(texts: List[str]) -> float:
        """Unsupervised target: diversity of token multiset (no gold labels)."""
        if not texts:
            return 0.0
        toks: List[str] = []
        for t in texts:
            toks.extend(str(t).lower().split())
        if not toks:
            return 0.0
        freq: Dict[str, int] = {}
        for w in toks:
            freq[w] = freq.get(w, 0) + 1
        total = len(toks)
        h = -sum((c / total) * math.log2(c / total) for c in freq.values() if c > 0)
        return float(max(0.0, min(1.0, h / (math.log2(max(len(freq), 2)) + 1e-8))))

    def fit_from_samples(self, hidden_refs: List[Any], texts_per_sample: List[List[str]]) -> None:
        """``hidden_refs[k]`` pairs with ``texts_per_sample[k]`` = list of generated strings."""
        xs: List[List[float]] = []
        ys: List[float] = []
        for h, tlist in zip(hidden_refs, texts_per_sample):
            v = _feature_vec(h, max_dim=self.dim)
            if len(v) < self.dim:
                v = v + [0.0] * (self.dim - len(v))
            xs.append(v[: self.dim])
            ys.append(self.pseudo_semantic_entropy(tlist))
        if len(xs) < 2:
            self.fitted = False
            return
        w = _ridge_fit(xs, ys, alpha=1e-3)
        self.weight = w
        preds = [sum(wi * xi for wi, xi in zip(w, xv)) for xv in xs]
        self.bias = sum(y - pr for y, pr in zip(ys, preds)) / len(ys)
        self.fitted = True

    def predict(self, hidden_state: Any) -> float:
        v = _feature_vec(hidden_state, max_dim=self.dim)
        if len(v) < self.dim:
            v = v + [0.0] * (self.dim - len(v))
        p = sum(w * x for w, x in zip(self.weight, v[: self.dim])) + self.bias
        return float(max(0.0, min(1.0, p)))

    def score_hidden_tensor(self, h: Any) -> float:
        """σ-like stress: predicted semantic entropy mapped to risk (high unknown → high σ)."""
        pred = self.predict(h)
        return float(max(0.0, min(1.0, pred if self.fitted else 0.45 + 0.55 * pred)))


def _ridge_fit(xs: List[List[float]], ys: List[float], *, alpha: float) -> List[float]:
    d = len(xs[0])
    a_mat = [[0.0] * d for _ in range(d)]
    b_vec = [0.0] * d
    for xv, y in zip(xs, ys):
        for i in range(d):
            b_vec[i] += xv[i] * y
            for j in range(d):
                a_mat[i][j] += xv[i] * xv[j]
    for i in range(d):
        a_mat[i][i] += float(alpha)
    return _gaussian_solve_square(a_mat, b_vec)


def _gaussian_solve_square(a: List[List[float]], b: List[float]) -> List[float]:
    n = len(b)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for col in range(n):
        piv = max(range(col, n), key=lambda r: abs(m[r][col]))
        m[col], m[piv] = m[piv], m[col]
        div = m[col][col] or 1e-8
        for j in range(col, n + 1):
            m[col][j] /= div
        for row in range(n):
            if row == col:
                continue
            f = m[row][col]
            for j in range(col, n + 1):
                m[row][j] -= f * m[col][j]
    return [m[i][n] for i in range(n)]


class SpectralProbe:
    """Spectral / low-rank cue: dominant eigenvalue share of layer Gram (attention optional)."""

    @staticmethod
    def score_attention_matrix(attn: List[List[float]]) -> float:
        """``attn`` square positive; returns stress in ``[0,1]`` (concentrated spectrum)."""
        n = len(attn)
        if n < 2:
            return 0.5
        lam = _power_iteration_symmetric(attn, steps=24)
        tr = sum(attn[i][i] for i in range(n)) + 1e-8
        ratio = lam / tr if tr > 0 else 0.0
        return float(max(0.0, min(1.0, ratio)))

    @staticmethod
    def score_hidden_stack(hidden_states: Any) -> float:
        hs = SignalCascade.as_layer_list(hidden_states)
        if len(hs) < 2:
            return 0.5
        rows: List[List[float]] = []
        for h in hs:
            rows.append(_feature_vec(h, max_dim=min(16, max(4, len(hs)))))
        # Small layer Gram G_ij = dot(row_i, row_j)
        ln = len(rows)
        g = [[0.0] * ln for _ in range(ln)]
        for i in range(ln):
            for j in range(ln):
                g[i][j] = sum(rows[i][k] * rows[j][k] for k in range(len(rows[i])))
        lam = _power_iteration_symmetric(g, steps=20)
        tr = sum(g[k][k] for k in range(ln)) + 1e-8
        ratio = lam / tr
        return float(max(0.0, min(1.0, ratio)))


def _power_iteration_symmetric(a: List[List[float]], *, steps: int) -> float:
    n = len(a)
    v = [1.0 / n**0.5] * n
    for _ in range(steps):
        nv = [sum(a[i][j] * v[j] for j in range(n)) for i in range(n)]
        norm = math.sqrt(sum(x * x for x in nv)) or 1e-8
        v = [x / norm for x in nv]
    lam = sum(v[i] * sum(a[i][j] * v[j] for j in range(n)) for i in range(n))
    return float(max(0.0, lam))


def ensemble_score(
    scores: Dict[str, float],
    weights: Optional[Dict[str, float]] = None,
) -> Dict[str, Any]:
    """Weighted probe fusion; calibrate ``weights`` with held-out ECE (external loop)."""
    default_w = {
        "L1_entropy": 0.2,
        "L2_hide": 0.2,
        "ICR": 0.18,
        "SEP": 0.15,
        "spectral": 0.15,
        "energy": 0.12,
    }
    wmap = weights or default_w
    num = 0.0
    den = 0.0
    for k, val in scores.items():
        wt = float(wmap.get(k, 0.0))
        num += wt * float(val)
        den += wt
    fused = float(num / den) if den > 0 else float(sum(scores.values()) / max(len(scores), 1))
    return {
        "sigma_ensemble": round(max(0.0, min(1.0, fused)), 6),
        "weights_used": dict(wmap),
        "note": "Weight keys must be calibrated per dataset; ensemble mitigates single-probe "
        "generalization failure (e.g. TruthfulQA vs HaluEval).",
    }


__all__ = [
    "EnergyProbe",
    "ICRProbe",
    "SEPProbe",
    "SignalCascade",
    "SigmaProbe",
    "SpectralProbe",
    "ensemble_score",
]

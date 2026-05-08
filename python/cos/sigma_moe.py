# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-MoE — Mixture of Experts where the **σ-gate** selects outputs by reliability, not a softmax router.

Does not modify ``sigma_gate.h``. Lab code: experts expose ``generate``; layer experts expose ``forward``.

**Token router (content gating):** :class:`SigmaMoETokenRouter` + :class:`MoETokenExpert` score each
(specialty, token) pair with the same gate — top-k by lowest σ, inverse-σ weights. Distinct from
:class:`SigmaMoE` generate-based routing above.
"""
from __future__ import annotations

import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple

Array = Any


def _normalize_verdict(v: Any) -> str:
    s = str(v).strip().upper()
    if "." in s:
        s = s.rsplit(".", 1)[-1]
    if s in ("ACCEPT", "RETHINK", "ABSTAIN"):
        return s
    return "ABSTAIN" if "ABSTAIN" in s else "RETHINK" if "RETHINK" in s else "ACCEPT" if "ACCEPT" in s else s


def _gate_score(gate: Any, prompt: str, response: str) -> Tuple[float, str]:
    score = getattr(gate, "score", None)
    if callable(score):
        sigma, verdict = score(prompt, response)
        return float(sigma), _normalize_verdict(verdict)
    call = getattr(gate, "__call__", None)
    if callable(call):
        sigma, verdict = call(None, None, prompt, response)
        return float(sigma), _normalize_verdict(verdict)
    raise TypeError("gate must implement score() or __call__")


def _default_feature_sigma(out: Array) -> float:
    try:
        import numpy as np  # optional dependency
    except ImportError:
        np = None  # type: ignore[misc, assignment]
    if np is not None and isinstance(out, np.ndarray):
        nrm = float(np.linalg.norm(out.astype("float64")))
        dim = max(int(np.size(out)), 1)
        return float(np.clip(nrm / (math.sqrt(dim) + 1e-9), 0.0, 1.0))
    if isinstance(out, (list, tuple)) and out:
        return float(abs(hash(str(out))) % 10000) / 10000.0
    if isinstance(out, (int, float)):
        return float(abs(float(out)) % 1.0)
    return 0.5


def _layer_sigma(gate: Any, out: Array) -> float:
    mf = getattr(gate, "measure_features", None)
    if callable(mf):
        return float(mf(out))
    return _default_feature_sigma(out)


def _add_scaled(acc: Optional[Array], term: Array, w: float) -> Array:
    if acc is None:
        return term * w
    try:
        import numpy as np

        if isinstance(term, np.ndarray):
            return acc + term * w
    except ImportError:
        if isinstance(term, (int, float)) and isinstance(acc, (int, float)):
            return float(acc) + float(term) * w
    return acc + term * w  # type: ignore[operator]


class SigmaMoE:
    """Route among text experts using ``gate.score``; optional ``forward``-style layer blend."""

    def __init__(
        self,
        experts: Sequence[Any],
        gate: Any,
        *,
        fallback_expert: Optional[Any] = None,
        early_accept_sigma: float = 0.1,
    ) -> None:
        self.experts = list(experts)
        self.gate = gate
        self.fallback = fallback_expert
        self.early_accept_sigma = float(early_accept_sigma)
        self.expert_stats: Dict[int, Dict[str, Any]] = {
            i: {"uses": 0, "avg_sigma": 0.5} for i in range(len(self.experts))
        }

    def update_stats(self, expert_id: int, sigma: float) -> None:
        stats = self.expert_stats[int(expert_id)]
        stats["uses"] = int(stats["uses"]) + 1
        alpha = 0.05
        stats["avg_sigma"] = float(alpha) * float(sigma) + (1.0 - float(alpha)) * float(stats["avg_sigma"])

    def route(self, prompt: str) -> Dict[str, Any]:
        ranked = sorted(self.expert_stats.items(), key=lambda x: float(x[1]["avg_sigma"]))
        best_result: Optional[str] = None
        best_sigma = 1.0
        best_expert: Optional[int] = None

        for expert_id, _stats in ranked:
            gen = getattr(self.experts[expert_id], "generate", None)
            if not callable(gen):
                continue
            response = str(gen(prompt))
            sigma, verdict = _gate_score(self.gate, prompt, response)
            self.update_stats(expert_id, sigma)
            if verdict == "ABSTAIN":
                continue
            if verdict == "RETHINK":
                continue
            if verdict == "ACCEPT" and sigma < best_sigma:
                best_result = response
                best_sigma = sigma
                best_expert = expert_id
                if sigma < self.early_accept_sigma:
                    break

        if best_result is None:
            if self.fallback is not None:
                fg = getattr(self.fallback, "generate", None)
                if callable(fg):
                    response = str(fg(prompt))
                    sigma, verdict = _gate_score(self.gate, prompt, response)
                    name = getattr(self.fallback, "name", "fallback")
                    return {
                        "response": response,
                        "expert": str(name),
                        "expert_id": None,
                        "sigma": sigma,
                        "verdict": verdict,
                    }
            return {
                "response": None,
                "expert": None,
                "expert_id": None,
                "sigma": 1.0,
                "verdict": "ABSTAIN",
            }

        ex = self.experts[best_expert if best_expert is not None else 0]
        ex_name = getattr(ex, "name", str(best_expert))
        return {
            "response": best_result,
            "expert": ex_name,
            "expert_id": best_expert,
            "sigma": best_sigma,
            "verdict": "ACCEPT",
        }

    def layer_moe(self, hidden_state: Array, layer_experts: Sequence[Any], top_k: int = 2) -> Array:
        k = max(1, int(top_k))
        outputs: List[Tuple[Array, float, Any]] = []
        for expert in list(layer_experts)[:k]:
            forward = getattr(expert, "forward", None)
            if not callable(forward):
                raise TypeError("layer expert must provide forward(hidden_state)")
            out = forward(hidden_state)
            sigma = _layer_sigma(self.gate, out)
            outputs.append((out, sigma, expert))

        weights = [1.0 / (float(s) + 0.01) for _, s, _ in outputs]
        total_w = sum(weights) or 1.0
        mixed: Optional[Array] = None
        for (out, _s, _e), w in zip(outputs, weights):
            mixed = _add_scaled(mixed, out, float(w) / total_w)
        assert mixed is not None
        return mixed

    def expert_health(self) -> Dict[str, Any]:
        usage_counts = [int(self.expert_stats[i]["uses"]) for i in range(len(self.experts))]
        total = sum(usage_counts) or 1
        distribution = [c / total for c in usage_counts]
        n = len(self.experts)
        entropy = -sum(p * math.log(p + 1e-10) for p in distribution)
        max_entropy = math.log(n) if n > 0 else 0.0
        balance = (entropy / max_entropy) if max_entropy > 0 else 0.0
        names = [getattr(self.experts[i], "name", str(i)) for i in range(len(self.experts))]
        return {
            "n_experts": n,
            "usage_distribution": [round(float(p), 6) for p in distribution],
            "balance": round(float(balance), 3),
            "collapsed": bool(balance < 0.3),
            "expert_sigmas": {names[i]: round(float(self.expert_stats[i]["avg_sigma"]), 3) for i in range(n)},
            "expert_uses": {names[i]: usage_counts[i] for i in range(n)},
        }


@dataclass
class LabExpert:
    name: str
    pattern: str

    def generate(self, prompt: str) -> str:
        p = prompt.lower()
        if self.pattern == "weak":
            if "quantum" in p:
                return "maybe quantum is like magic or something uncertain"
            return "uh uncertain answer"
        if self.pattern == "strong":
            if "quantum" in p:
                return "Quantum entanglement is correlation of outcomes between separated systems."
            return "Concise factual reply."
        if self.pattern == "abstain":
            return "nonsense @@@"
        return "ok"


class LabMoEGate:
    """Small gate for σ-MoE tests (aligned with :class:`LabCostGate` heuristics)."""

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        p, r = prompt.lower(), response.lower()
        if "@@@" in r or "nonsense" in r:
            return 0.95, "ABSTAIN"
        if "quantum" in p:
            if "entanglement" in r and "maybe" not in r and "uncertain" not in r and "magic" not in r:
                return 0.06, "ACCEPT"
            if "maybe" in r or "uncertain" in r or "magic" in r:
                return 0.55, "RETHINK"
        if len(r.split()) <= 2:
            return 0.6, "RETHINK"
        return 0.08, "ACCEPT"


def default_moe_state_path() -> Path:
    d = Path.home() / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "sigma_moe_lab.json"


def load_moe_state(
    path: Path,
    expert_names: Sequence[str],
    expert_patterns: Optional[Mapping[str, str]] = None,
) -> SigmaMoE:
    """Rebuild :class:`SigmaMoE` with stats from ``path`` (if present)."""
    patterns = dict(expert_patterns or {})
    experts: List[LabExpert] = [LabExpert(name=n, pattern=patterns.get(n, "strong")) for n in expert_names]
    moe = SigmaMoE(experts, LabMoEGate())
    if path.is_file():
        blob = json.loads(path.read_text(encoding="utf-8"))
        stats = blob.get("expert_stats") or {}
        if isinstance(stats, dict):
            for k, v in stats.items():
                try:
                    idx = int(k)
                except (TypeError, ValueError):
                    continue
                if idx in moe.expert_stats and isinstance(v, dict):
                    moe.expert_stats[idx] = {
                        "uses": int(v.get("uses", 0)),
                        "avg_sigma": float(v.get("avg_sigma", 0.5)),
                    }
    return moe


def save_moe_state(path: Path, moe: SigmaMoE) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    out = {"expert_stats": {str(i): dict(moe.expert_stats[i]) for i in range(len(moe.experts))}}
    path.write_text(json.dumps(out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def default_registry_path() -> Path:
    d = Path.home() / ".cos"
    d.mkdir(parents=True, exist_ok=True)
    return d / "moe_experts_registry.json"


def load_registry(path: Path) -> Dict[str, str]:
    if not path.is_file():
        return {}
    data = json.loads(path.read_text(encoding="utf-8"))
    if isinstance(data, dict):
        return {str(k): str(v) for k, v in data.items() if isinstance(v, str)}
    return {}


def save_registry(path: Path, reg: Mapping[str, str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(dict(reg), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


class MoETokenExpert:
    """Token-routing expert: specialty string + per-call σ stats (lab)."""

    def __init__(self, expert_id: int, specialty: str, gate: Any) -> None:
        self.id = int(expert_id)
        self.specialty = str(specialty)
        self.gate = gate
        self.calls = 0
        self.total_σ = 0.0

    def process(self, token: str, context: str = "") -> Dict[str, Any]:
        σ, verdict = _gate_score(self.gate, f"{self.specialty}: {context}", str(token))
        self.calls += 1
        self.total_σ += float(σ)
        return {
            "token": token,
            "σ": float(σ),
            "verdict": _normalize_verdict(verdict),
            "expert": self.id,
        }

    def avg_σ(self) -> float:
        return round(float(self.total_σ) / max(self.calls, 1), 4)


class SigmaMoETokenRouter:
    """σ-gate scores each (specialty, token) pair; lowest-σ experts win top-k (no auxiliary loss)."""

    def __init__(self, gate: Any = None, top_k: int = 2) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate if gate is not None else SigmaGate()
        self.experts: List[MoETokenExpert] = []
        self.top_k = max(1, int(top_k))
        self.routing_history: List[Dict[str, Any]] = []

    def add_expert(self, specialty: str) -> MoETokenExpert:
        ex = MoETokenExpert(len(self.experts), specialty, self.gate)
        self.experts.append(ex)
        return ex

    def route(self, token: str, context: str = "") -> Dict[str, Any]:
        if not self.experts:
            return {"error": "no experts"}

        scores: List[Dict[str, Any]] = []
        for expert in self.experts:
            σ, _verdict = _gate_score(
                self.gate,
                f"{expert.specialty} handles",
                f"{token} in {context}",
            )
            scores.append({"expert_id": expert.id, "σ": float(σ), "specialty": expert.specialty})

        scores.sort(key=lambda s: float(s["σ"]))
        selected = scores[: self.top_k]

        results: List[Dict[str, Any]] = []
        for s in selected:
            expert = self.experts[int(s["expert_id"])]
            result = expert.process(str(token), str(context))
            results.append(result)

        total_inv_σ = sum(1.0 / max(float(r["σ"]), 0.001) for r in results)
        for r in results:
            r["weight"] = round((1.0 / max(float(r["σ"]), 0.001)) / total_inv_σ, 4)

        best = min(results, key=lambda r: float(r["σ"]))
        routing: Dict[str, Any] = {
            "token": token,
            "selected_experts": [int(s["expert_id"]) for s in selected],
            "best_expert": best["expert"],
            "best_σ": round(float(best["σ"]), 4),
            "all_results": results,
        }
        self.routing_history.append(routing)
        return routing

    def load_balance(self) -> Dict[str, Any]:
        if not self.experts:
            return {"balanced": True, "per_expert": [], "imbalance": 0.0, "total_tokens": 0}

        calls = [int(e.calls) for e in self.experts]
        max_calls = max(calls) if calls else 0
        min_calls = min(calls) if calls else 0
        imbalance = (max_calls - min_calls) / max(max_calls, 1)

        return {
            "per_expert": [
                {"id": e.id, "specialty": e.specialty, "calls": e.calls, "avg_σ": e.avg_σ()}
                for e in self.experts
            ],
            "imbalance": round(float(imbalance), 4),
            "balanced": bool(imbalance < 0.5),
            "total_tokens": int(sum(calls)),
        }

    def expert_utilization(self) -> Dict[str, Any]:
        if not self.experts:
            return {
                "total": 0,
                "active": 0,
                "dead": [],
                "overloaded": [],
                "utilization": 1.0,
            }
        dead = [e for e in self.experts if e.calls == 0]
        mean_calls = sum(ex.calls for ex in self.experts) / max(len(self.experts), 1)
        overloaded = [e for e in self.experts if e.calls > mean_calls * 2]
        return {
            "total": len(self.experts),
            "active": len(self.experts) - len(dead),
            "dead": [e.id for e in dead],
            "overloaded": [e.id for e in overloaded],
            "utilization": round((len(self.experts) - len(dead)) / max(len(self.experts), 1), 4),
        }

    def dynamic_add_expert(self, sigma_trace: List[float], threshold: float = 0.6) -> Dict[str, Any]:
        """Heuristic: high recent σ ⇒ add an auto-tagged expert (lab hook)."""
        if not sigma_trace:
            return {"added": False}
        tail = [float(x) for x in sigma_trace[-20:]]
        avg = sum(tail) / max(len(tail), 1)
        if avg > float(threshold):
            new_expert = self.add_expert(f"auto_expert_{len(self.experts)}")
            return {
                "added": True,
                "expert_id": new_expert.id,
                "reason": f"avg σ={avg:.3f} > {threshold} → new expert needed",
            }
        return {"added": False, "avg_σ": round(avg, 4)}


__all__ = [
    "SigmaMoE",
    "MoETokenExpert",
    "SigmaMoETokenRouter",
    "LabExpert",
    "LabMoEGate",
    "default_moe_state_path",
    "load_moe_state",
    "save_moe_state",
    "default_registry_path",
    "load_registry",
    "save_registry",
]

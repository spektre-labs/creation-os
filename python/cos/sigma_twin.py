# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-twin lab — offline copy of a σ-shaped **control plane** (gate knobs + toy model + engram).

This is **not** a plant-floor digital twin and does **not** claim industrial fault-prediction rates.
``python/cos/sigma_gate.h`` is not modified; scoring uses ``sigma_gate_core`` on deterministic stress.

Production and twin are **separate deep copies** — twin experiments do not mutate production until
``promote_twin`` succeeds under stated policy checks.
"""
from __future__ import annotations

import copy
import hashlib
import json
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Mapping, MutableMapping, Optional, Sequence, Tuple, Union

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_q16, sigma_update_q16

CHANGE_PREFIX = "σ_twin_lab:"
DEFAULT_FIXTURE: List[Tuple[str, str]] = [
    ("What is 2+2?", "4"),
    ("Capital of France?", "Paris"),
    ("Is the sky green?", "no"),
    ("Water boils at 100C at 1 atm. Yes?", "yes"),
]


@dataclass
class TwinGateLab:
    """Tunable knobs that feed a deterministic stress scalar into ``sigma_gate_core``."""

    k_raw: float = 0.92
    sigma_scale: float = 1.0
    stress_bias: float = 0.0
    # Pedagogical aliases (not ``sigma_gate.h`` thresholds):
    threshold_accept: float = 0.35
    threshold_abstain: float = 0.78
    ema_decay: float = 0.92  # aliased to k_raw when set via apply_changes

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        raw = hashlib.sha256(f"{prompt}\n{response}".encode("utf-8")).digest()
        u = int.from_bytes(raw[:4], "big") / float(2**32)
        stress = (u * self.sigma_scale) + self.stress_bias
        stress -= max(0.0, 0.25 - self.threshold_accept) * 0.15
        if stress > self.threshold_abstain * 0.9:
            stress = min(1.0, stress + 0.05)
        stress = max(0.0, min(1.0, stress))
        st = SigmaState()
        sigma_update_q16(st, sigma_q16(stress), sigma_q16(self.k_raw))
        verdict = Verdict(int(sigma_gate(st))).name
        return float(stress), verdict

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Any = None,
    ) -> float:
        """Match :class:`~cos.sigma_gate.SigmaGate` arity for evolve / MoE callers."""
        del model, tokenizer, reference
        s, _ = self.score(str(prompt), str(response))
        return float(s)


@dataclass
class TwinEngramLab:
    tau: float = 0.3
    entries: List[Tuple[str, float]] = field(default_factory=list)


class TwinModelLab:
    """Deterministic pseudo-responses (no external LLM)."""

    def __init__(self, *, style: str = "default") -> None:
        self.style = str(style)

    def generate(self, prompt: str) -> str:
        h = hashlib.sha256(prompt.encode("utf-8")).hexdigest()[:16]
        if self.style == "short":
            return f"s:{h[:6]}"
        if self.style == "verbose":
            return f"longform:{h}:{prompt[:24]}"
        return f"resp:{h}"


def check_correct(response: str, expected: str) -> bool:
    e = expected.strip().lower()
    r = response.strip().lower()
    if not e:
        return True
    return e in r or r in e


def load_test_prompts(path: Optional[Union[str, Path]]) -> List[Tuple[str, str]]:
    if not path:
        return list(DEFAULT_FIXTURE)
    p = Path(path).expanduser()
    if not p.is_file():
        return list(DEFAULT_FIXTURE)
    text = p.read_text(encoding="utf-8", errors="replace").strip()
    if not text:
        return list(DEFAULT_FIXTURE)
    rows: List[Tuple[str, str]] = []
    if text.startswith("["):
        data = json.loads(text)
        for item in data:
            if isinstance(item, dict):
                pr = str(item.get("prompt") or item.get("question") or "")
                exp = str(item.get("answer") or item.get("expected") or item.get("completion") or "")
                if pr.strip():
                    rows.append((pr.strip(), exp.strip()))
        return rows or list(DEFAULT_FIXTURE)
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(obj, dict):
            pr = str(obj.get("prompt") or obj.get("question") or "")
            exp = str(obj.get("answer") or obj.get("expected") or "")
            if pr.strip():
                rows.append((pr.strip(), exp.strip()))
    return rows or list(DEFAULT_FIXTURE)


class SigmaTwin:
    def __init__(
        self,
        gate: TwinGateLab,
        model: TwinModelLab,
        engram: TwinEngramLab,
    ) -> None:
        self.production: Dict[str, Any] = {"gate": gate, "model": model, "engram": engram}
        self.twin: Optional[Dict[str, Any]] = None
        self.experiments: List[Dict[str, Any]] = []

    def create_twin(self) -> Dict[str, Any]:
        self.twin = {
            "gate": copy.deepcopy(self.production["gate"]),
            "model": TwinModelLab(style=self.production["model"].style),
            "engram": copy.deepcopy(self.production["engram"]),
        }
        return {"created": True, "snapshot": self.snapshot(self.twin)}

    def snapshot(self, system: Mapping[str, Any]) -> Dict[str, Any]:
        g: TwinGateLab = system["gate"]
        eg: TwinEngramLab = system["engram"]
        return {
            "gate": asdict(g),
            "model_style": system["model"].style,
            "engram_tau": eg.tau,
            "engram_size": len(eg.entries),
        }

    def apply_changes(self, system: MutableMapping[str, Any], changes: Mapping[str, Any]) -> None:
        gate: TwinGateLab = system["gate"]
        if "threshold_accept" in changes:
            gate.threshold_accept = float(changes["threshold_accept"])
        if "threshold_abstain" in changes:
            gate.threshold_abstain = float(changes["threshold_abstain"])
        if "ema_decay" in changes:
            gate.ema_decay = float(changes["ema_decay"])
            gate.k_raw = float(changes["ema_decay"])
        if "k_raw" in changes:
            gate.k_raw = float(changes["k_raw"])
        if "sigma_scale" in changes:
            gate.sigma_scale = float(changes["sigma_scale"])
        if "stress_bias" in changes:
            gate.stress_bias = float(changes["stress_bias"])
        if "engram_tau" in changes:
            system["engram"].tau = float(changes["engram_tau"])
        if "model" in changes:
            m = changes["model"]
            if isinstance(m, TwinModelLab):
                system["model"] = m
            else:
                system["model"] = TwinModelLab(style=str(m))

    def run_tests(self, system: Mapping[str, Any], test_prompts: Sequence[Tuple[str, str]]) -> Dict[str, Any]:
        results: List[Dict[str, Any]] = []
        gate: TwinGateLab = system["gate"]
        model: TwinModelLab = system["model"]
        for prompt, expected in test_prompts:
            resp = model.generate(prompt)
            sigma, verdict = gate.score(prompt, resp)
            results.append(
                {
                    "prompt": prompt[:80],
                    "sigma": round(float(sigma), 6),
                    "verdict": verdict,
                    "correct": check_correct(resp, expected),
                }
            )
        n = max(len(results), 1)
        return {
            "results": results,
            "avg_sigma": sum(float(r["sigma"]) for r in results) / n,
            "accuracy": sum(1 for r in results if r["correct"]) / n,
            "abstain_rate": sum(1 for r in results if r["verdict"] == "ABSTAIN") / n,
        }

    def compare(self, prod: Mapping[str, Any], twin_out: Mapping[str, Any]) -> Dict[str, Any]:
        ps = float(prod["avg_sigma"])
        ts = float(twin_out["avg_sigma"])
        deploy = ts < ps and float(twin_out["accuracy"]) >= float(prod["accuracy"])
        return {
            "sigma_delta": ts - ps,
            "accuracy_delta": float(twin_out["accuracy"]) - float(prod["accuracy"]),
            "abstain_delta": float(twin_out["abstain_rate"]) - float(prod["abstain_rate"]),
            "recommendation": "DEPLOY" if deploy else "KEEP_CURRENT",
        }

    def experiment(
        self,
        name: str,
        changes: Mapping[str, Any],
        test_prompts: Sequence[Tuple[str, str]],
    ) -> Dict[str, Any]:
        if not self.twin:
            self.create_twin()
        assert self.twin is not None
        twin_branch = {
            "gate": copy.deepcopy(self.twin["gate"]),
            "model": TwinModelLab(style=self.twin["model"].style),
            "engram": copy.deepcopy(self.twin["engram"]),
        }
        self.apply_changes(twin_branch, changes)
        prod_branch = {
            "gate": copy.deepcopy(self.production["gate"]),
            "model": TwinModelLab(style=self.production["model"].style),
            "engram": copy.deepcopy(self.production["engram"]),
        }
        twin_results = self.run_tests(twin_branch, test_prompts)
        prod_results = self.run_tests(prod_branch, test_prompts)
        comparison = self.compare(prod_results, twin_results)
        exp = {
            "name": name,
            "changes": dict(changes),
            "twin_avg_sigma": round(twin_results["avg_sigma"], 6),
            "prod_avg_sigma": round(prod_results["avg_sigma"], 6),
            "delta_sigma": round(twin_results["avg_sigma"] - prod_results["avg_sigma"], 6),
            "improvement": bool(twin_results["avg_sigma"] < prod_results["avg_sigma"]),
            "twin_accuracy": twin_results["accuracy"],
            "prod_accuracy": prod_results["accuracy"],
            "details": comparison,
        }
        self.experiments.append(exp)
        self.twin = {
            "gate": copy.deepcopy(twin_branch["gate"]),
            "model": TwinModelLab(style=twin_branch["model"].style),
            "engram": copy.deepcopy(twin_branch["engram"]),
        }
        return exp

    def promote_twin(self) -> Dict[str, Any]:
        if not self.twin:
            return {"promoted": False, "reason": "no twin snapshot"}
        last = self.experiments[-1] if self.experiments else None
        if not last:
            return {"promoted": False, "reason": "no experiments"}
        if not last.get("improvement"):
            return {"promoted": False, "reason": "last experiment did not lower σ"}
        if last.get("details", {}).get("recommendation") != "DEPLOY":
            return {
                "promoted": False,
                "reason": last.get("details", {}).get("recommendation", "KEEP_CURRENT"),
            }
        self.production = {
            "gate": copy.deepcopy(self.twin["gate"]),
            "model": TwinModelLab(style=self.twin["model"].style),
            "engram": copy.deepcopy(self.twin["engram"]),
        }
        last["deployed"] = True
        return {
            "promoted": True,
            "experiment": last["name"],
            "delta_sigma": last["delta_sigma"],
        }

    def what_if(
        self,
        question: str,
        changes: Mapping[str, Any],
        test_prompts: Sequence[Tuple[str, str]],
    ) -> Dict[str, Any]:
        return self.experiment(f"what_if_{question}", changes, test_prompts)

    def to_json_obj(self) -> Dict[str, Any]:
        def ser_system(sym: Mapping[str, Any]) -> Dict[str, Any]:
            return {
                "gate": asdict(sym["gate"]),
                "model_style": sym["model"].style,
                "engram": {"tau": sym["engram"].tau, "entries": sym["engram"].entries},
            }

        return {
            "version": 1,
            "prefix": CHANGE_PREFIX,
            "production": ser_system(self.production),
            "twin": ser_system(self.twin) if self.twin else None,
            "experiments": list(self.experiments),
        }

    @classmethod
    def from_json_obj(cls, obj: Mapping[str, Any]) -> SigmaTwin:
        def deser(d: Mapping[str, Any]) -> Dict[str, Any]:
            g = TwinGateLab(**dict(d["gate"]))
            m = TwinModelLab(style=str(d.get("model_style", "default")))
            egd = d.get("engram") or {}
            entries = []
            for x in egd.get("entries") or []:
                if isinstance(x, (list, tuple)) and len(x) >= 2:
                    entries.append((str(x[0]), float(x[1])))
            eg = TwinEngramLab(tau=float(egd.get("tau", 0.3)), entries=entries)
            return {"gate": g, "model": m, "engram": eg}

        prod = deser(obj["production"])
        inst = cls(prod["gate"], prod["model"], prod["engram"])
        tw = obj.get("twin")
        if tw:
            inst.twin = deser(tw)
        inst.experiments = list(obj.get("experiments") or [])
        return inst

    def save(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.to_json_obj(), indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    @classmethod
    def load(cls, path: Path) -> SigmaTwin:
        obj = json.loads(path.read_text(encoding="utf-8"))
        return cls.from_json_obj(obj)


def parse_change_map(pairs: Sequence[str]) -> Dict[str, Any]:
    out: Dict[str, Any] = {}
    for p in pairs:
        p = str(p).strip()
        if not p or "=" not in p:
            continue
        k, v = p.split("=", 1)
        k, v = k.strip(), v.strip()
        try:
            if "." in v or "e" in v.lower():
                out[k] = float(v)
            else:
                out[k] = int(v)
        except ValueError:
            out[k] = v
    return out


def default_sigma_twin() -> SigmaTwin:
    return SigmaTwin(TwinGateLab(), TwinModelLab(), TwinEngramLab())


def workspace_state_path(workspace: Path) -> Path:
    return workspace / "sigma_twin_lab.json"


__all__ = [
    "SigmaTwin",
    "TwinEngramLab",
    "TwinGateLab",
    "TwinModelLab",
    "default_sigma_twin",
    "load_test_prompts",
    "parse_change_map",
    "workspace_state_path",
]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-LoRA — multi-adapter registry, σ-guided selection, gated merge (Python lab).

This module does **not** ship a full HF/PEFT training stack by default: ``train_adapter``
can call PEFT when installed; CI and quickstart use :meth:`train_adapter_synthetic`.

Does not modify ``sigma_gate.h``. See ``docs/CLAIM_DISCIPLINE.md`` for claim hygiene.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional, Protocol, Tuple, Union, runtime_checkable

from .sigma_gate_core import Verdict

DEFAULT_LORA_RANK = 16
DEFAULT_LORA_ALPHA = 32  # convention: 2× rank for Llama-style stacks
DEFAULT_TARGET_MODULES = ("q_proj", "v_proj")
SELECT_PROBE_TOP = 3


def _verdict_label(v: Union[str, Verdict]) -> str:
    if isinstance(v, Verdict):
        return v.name
    s = str(v).upper()
    for name in ("ACCEPT", "RETHINK", "ABSTAIN"):
        if name in s:
            return name
    return "RETHINK"


@runtime_checkable
class LoRAGate(Protocol):
    def score(self, prompt: str, response: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class LoRABase(Protocol):
    """Base model with PEFT-style hot-swap hooks (optional merge)."""

    def generate(self, prompt: str) -> str:
        ...

    def load_adapter(self, path: str, **kwargs: Any) -> None:
        ...

    def unload_adapter(self, name: Optional[str] = None) -> None:
        ...


class SigmaLoRA:
    """Register adapters, σ-pick among candidates, infer, merge only if σ is low enough."""

    def __init__(
        self,
        base_model: LoRABase,
        gate: LoRAGate,
        *,
        merge_sigma_max: float = 0.3,
    ) -> None:
        self.base = base_model
        self.gate = gate
        self.merge_sigma_max = float(merge_sigma_max)
        self.adapters: Dict[str, Dict[str, Any]] = {}
        self.adapter_stats: Dict[str, Dict[str, Any]] = {}

    def register(self, name: str, adapter_path: str, domain: Optional[str] = None) -> None:
        self.adapters[name] = {
            "path": str(adapter_path),
            "domain": domain,
            "loaded": False,
        }
        self.adapter_stats.setdefault(
            name,
            {"uses": 0, "avg_sigma": 0.5, "best_sigma": 1.0},
        )

    def match_domain(self, prompt: str) -> List[str]:
        if not self.adapters:
            return []
        p = prompt.lower()
        hits: List[str] = []
        for name, meta in self.adapters.items():
            dom = (meta.get("domain") or "").lower().strip()
            if dom and dom in p:
                hits.append(name)
            safe = p.replace("-", "_").replace(" ", "_")
            if name.lower() in safe.split("_") or name.lower() in p:
                hits.append(name)
        if hits:
            return list(dict.fromkeys(hits))
        return list(self.adapters.keys())

    def load_adapter(self, name: str) -> None:
        meta = self.adapters[name]
        if meta["loaded"]:
            return
        self.base.load_adapter(meta["path"], name=name)
        meta["loaded"] = True

    def unload_adapter(self, name: str) -> None:
        meta = self.adapters.get(name)
        if not meta or not meta.get("loaded"):
            return
        self.base.unload_adapter(name)
        meta["loaded"] = False

    def validate_adapter(self, adapter_name: str, prompts: List[str]) -> float:
        if not prompts:
            return 1.0
        self.load_adapter(adapter_name)
        sigmas: List[float] = []
        try:
            for p in prompts:
                r = self.base.generate(p)
                s, _ = self.gate.score(p, r)
                sigmas.append(float(s))
        finally:
            self.unload_adapter(adapter_name)
        return float(sum(sigmas) / max(len(sigmas), 1))

    def select_adapter(self, prompt: str) -> Optional[str]:
        if not self.adapters:
            return None
        candidates = self.match_domain(prompt)
        if len(candidates) == 1:
            return candidates[0]

        scores: List[Tuple[str, float, str]] = []
        for name in candidates[:SELECT_PROBE_TOP]:
            self.load_adapter(name)
            try:
                response = self.base.generate(prompt)
                sigma, verdict = self.gate.score(prompt, response)
                scores.append((name, float(sigma), _verdict_label(verdict)))
            finally:
                self.unload_adapter(name)

        if not scores:
            return None
        best = min(scores, key=lambda x: x[1])
        return best[0]

    def inference_with_adapter(self, prompt: str, adapter_name: Optional[str] = None) -> Dict[str, Any]:
        picked = adapter_name or self.select_adapter(prompt)
        if picked:
            self.load_adapter(picked)
        try:
            response = self.base.generate(prompt)
            sigma, verdict = self.gate.score(prompt, response)
            vn = _verdict_label(verdict)
            if picked:
                st = self.adapter_stats[picked]
                st["uses"] = int(st["uses"]) + 1
                u = int(st["uses"])
                st["avg_sigma"] = (float(st["avg_sigma"]) * (u - 1) + float(sigma)) / max(u, 1)
                st["best_sigma"] = min(float(st["best_sigma"]), float(sigma))
        finally:
            if picked:
                self.unload_adapter(picked)

        return {
            "response": response,
            "sigma": float(sigma),
            "verdict": vn,
            "adapter": picked,
        }

    def compare_adapters(self, names: List[str], prompt: str) -> Dict[str, Any]:
        rows: List[Dict[str, Any]] = []
        winner: Optional[str] = None
        best_sigma = 1e9
        for name in names:
            if name not in self.adapters:
                continue
            self.load_adapter(name)
            try:
                r = self.base.generate(prompt)
                s, v = self.gate.score(prompt, r)
                vn = _verdict_label(v)
                rows.append({"adapter": name, "sigma": float(s), "verdict": vn})
                if float(s) < best_sigma:
                    best_sigma = float(s)
                    winner = name
            finally:
                self.unload_adapter(name)
        return {"prompt": prompt, "results": rows, "winner": winner, "best_sigma": best_sigma}

    def merge_adapter(
        self,
        name: str,
        *,
        validation_prompts: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        prompts = validation_prompts or ["Summarize the task in one sentence."]
        val_sigma = self.validate_adapter(name, prompts)
        if val_sigma > self.merge_sigma_max:
            return {
                "merged": False,
                "reason": f"adapter σ too high: {val_sigma:.3f}",
                "sigma": float(val_sigma),
            }
        merger = getattr(self.base, "merge_and_unload", None)
        if not callable(merger):
            return {
                "merged": False,
                "reason": "base does not implement merge_and_unload",
                "sigma": float(val_sigma),
            }
        merger(name)
        return {"merged": True, "sigma": float(val_sigma)}

    def train_adapter_synthetic(
        self,
        name: str,
        *,
        adapter_path: str,
        val_sigma_by_epoch: List[float],
        epochs: int = 3,
        sigma_stop: float = 0.5,
        domain: Optional[str] = None,
    ) -> Dict[str, Any]:
        """Lab-only epoch loop: stop when validation σ exceeds ``sigma_stop``."""
        self.register(name, adapter_path, domain=domain)
        last_sigma = 0.0
        stopped_epoch: Optional[int] = None
        early = False
        for e in range(int(epochs)):
            last_sigma = float(val_sigma_by_epoch[e] if e < len(val_sigma_by_epoch) else last_sigma)
            if last_sigma > float(sigma_stop):
                stopped_epoch = e
                early = True
                break
        if stopped_epoch is None:
            stopped_epoch = int(epochs) - 1
        return {
            "name": name,
            "path": adapter_path,
            "final_sigma": last_sigma,
            "stopped_epoch": stopped_epoch,
            "early_stop": early,
            "note": "synthetic σ curve (no PEFT weights updated)",
        }

    def train_adapter(
        self,
        name: str,
        dataset: Any = None,
        *,
        rank: int = DEFAULT_LORA_RANK,
        lr: float = 1e-4,
        epochs: int = 3,
        validation_prompts: Optional[List[str]] = None,
        save_dir: Optional[str] = None,
        target_modules: Optional[Tuple[str, ...]] = None,
    ) -> Dict[str, Any]:
        """Optional PEFT path when ``peft`` and a trainable ``base`` are available."""
        validation_prompts = validation_prompts or []
        save_dir = save_dir or f"adapters/{name}"
        try:
            from peft import LoraConfig, get_peft_model  # type: ignore
        except ImportError:
            return {
                "error": "peft_not_installed",
                "hint": "pip install peft transformers; or use train_adapter_synthetic / --mock in CLI",
                "name": name,
            }

        tm = list(target_modules or DEFAULT_TARGET_MODULES)
        config = LoraConfig(r=int(rank), lora_alpha=int(rank) * 2, target_modules=tm)
        try:
            model = get_peft_model(self.base, config)  # type: ignore[arg-type]
        except Exception as e:
            return {"error": "peft_wrap_failed", "message": str(e), "name": name}

        last_sigma = 0.0
        for epoch in range(int(epochs)):
            if dataset is None:
                break
            try:
                for batch in dataset:
                    if hasattr(model, "train_step"):
                        model.train_step(batch)  # type: ignore[attr-defined]
            except Exception as e:
                return {"error": "train_step_failed", "epoch": epoch, "message": str(e), "name": name}
            if validation_prompts:
                last_sigma = self._validate_peft_model(model, validation_prompts)
                if last_sigma > 0.5:
                    return {
                        "name": name,
                        "final_sigma": last_sigma,
                        "stopped_epoch": epoch,
                        "early_stop": True,
                        "note": "σ validation threshold 0.5",
                    }

        out_path = Path(save_dir)
        out_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            model.save_pretrained(str(out_path))  # type: ignore[attr-defined]
        except Exception as e:
            return {"error": "save_failed", "message": str(e), "name": name}

        self.register(name, str(out_path))
        return {"name": name, "final_sigma": last_sigma, "path": str(out_path), "early_stop": False}

    def _validate_peft_model(self, model: Any, prompts: List[str]) -> float:
        sigmas: List[float] = []
        for p in prompts:
            if hasattr(model, "generate"):
                r = model.generate(p)  # type: ignore[operator]
            else:
                r = self.base.generate(p)
            s, _ = self.gate.score(p, str(r))
            sigmas.append(float(s))
        return float(sum(sigmas) / max(len(sigmas), 1))

    def export_registry(self) -> Dict[str, Any]:
        return {
            "adapters": {
                k: {"path": v["path"], "domain": v.get("domain")} for k, v in self.adapters.items()
            },
            "stats": {k: dict(v) for k, v in self.adapter_stats.items()},
        }

    def import_registry(self, blob: Dict[str, Any]) -> None:
        for name, meta in (blob.get("adapters") or {}).items():
            if isinstance(meta, dict) and "path" in meta:
                self.register(str(name), str(meta["path"]), domain=meta.get("domain"))
        for name, st in (blob.get("stats") or {}).items():
            if name in self.adapter_stats and isinstance(st, dict):
                self.adapter_stats[name].update(st)


__all__ = [
    "DEFAULT_LORA_ALPHA",
    "DEFAULT_LORA_RANK",
    "DEFAULT_TARGET_MODULES",
    "SigmaLoRA",
]


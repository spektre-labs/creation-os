# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""MEGA — unified **lab** cognitive sweep wiring σ through optional Creation OS modules.

Perceive → ground → remember → predict → reason → plan → metacognition → emotion →
act → learn → convergence; offline :meth:`dream` for light consolidation hooks.

This is an **integration scaffold**, not a productized agent runtime and **not** strong RSI.
The entropy core stays in C (`sigma_gate.h`); Python **must not** mutate that boundary.
**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["Mega"]


def _verdict_norm(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    tail = raw.split(".")[-1] if "." in raw else raw
    return str(tail).strip().upper()


def _instantiate(module_path: str, cls_name: str, gate: Any) -> Any:
    mod = __import__(module_path, fromlist=[cls_name])
    cls = getattr(mod, cls_name)
    try:
        return cls(gate=gate)
    except TypeError:
        return cls()


class Mega:
    """One σ-centric step over optional subsystems (graceful import / API degradation)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.cycle = 0
        self.σ_trace: List[float] = []
        self._modules: Dict[str, Any] = {}
        self._boot()

    def _boot(self) -> None:
        """Load optional modules; failures become ``None`` (skipped in :meth:`step`)."""
        modules: Dict[str, tuple[str, str]] = {
            "predictive": ("cos.predictive", "PredictiveCoding"),
            "reason": ("cos.reason", "SigmaReason"),
            "quantum": ("cos.quantum_cognition", "QuantumCognition"),
            "memory": ("cos.memory", "SigmaMemory"),
            "engram": ("cos.engram", "Engram"),
            "graph": ("cos.graph", "SigmaGraph"),
            "planner": ("cos.planner", "SigmaPlanner"),
            "agency": ("cos.agency", "SigmaAgency"),
            "ttt": ("cos.ttt", "SigmaTTT"),
            "evolve": ("cos.evolve", "SigmaEvolve"),
            "conscious": ("cos.conscious", "SigmaConsciousV2"),
            "drive": ("cos.drive", "SigmaDrive"),
            "social": ("cos.social", "SigmaSocial"),
            "world": ("cos.world", "SigmaWorldV2"),
            "causal": ("cos.causal", "PearlLadder"),
            "convergence": ("cos.convergence", "SigmaConvergence"),
            "grounding": ("cos.grounding", "SigmaGrounding"),
        }
        for name, (mod_path, cls_name) in modules.items():
            try:
                self._modules[name] = _instantiate(mod_path, cls_name, self.gate)
            except Exception:
                self._modules[name] = None

    def _extract_σ(self, row: Dict[str, Any]) -> Optional[float]:
        for key in ("σ", "σ_meta", "sigma", "total_σ", "best_σ"):
            val = row.get(key)
            if isinstance(val, (int, float)):
                return float(val)
        return None

    def step(self, observation: Any, goal: Optional[str] = None) -> Dict[str, Any]:
        """Run one full sweep; σ from :meth:`~cos.sigma_gate.SigmaGate.score` drives gating."""
        self.cycle += 1
        trace: Dict[str, Any] = {"cycle": self.cycle, "stages": {}}

        σ_perceive, verdict_raw = self.gate.score("perceive", str(observation))
        verdict = _verdict_norm(verdict_raw)
        trace["stages"]["perceive"] = {
            "σ": round(float(σ_perceive), 4),
            "verdict": verdict,
        }

        gmod = self._modules.get("grounding")
        if gmod is not None:
            try:
                ground = gmod.hallucination_check(str(observation), str(observation))
                gn = float(ground.get("σ", ground.get("sigma", 0.5)))
                trace["stages"]["ground"] = {
                    "σ": round(gn, 4),
                    "grounded": bool(ground.get("grounded", False)),
                }
            except Exception:
                trace["stages"]["ground"] = {"σ": 0.5, "grounded": False, "error": True}

        mmem = self._modules.get("memory")
        if mmem is not None:
            try:
                memories = mmem.recall(str(observation))
                trace["stages"]["remember"] = {"retrieved": bool(memories)}
            except Exception:
                trace["stages"]["remember"] = {"retrieved": False}

        pred = self._modules.get("predictive")
        if pred is not None:
            try:
                pc = pred.full_cycle(str(observation))
                trace["stages"]["predict"] = {
                    "σ": float(pc.get("total_σ", 0.5)),
                }
            except Exception:
                trace["stages"]["predict"] = {"σ": 0.5}

        causal = self._modules.get("causal")
        if causal is not None and goal:
            try:
                see_row = causal.see(str(observation), str(goal))
                trace["stages"]["reason"] = {"σ": float(see_row.get("σ", 0.5))}
            except Exception:
                trace["stages"]["reason"] = {"σ": 0.5}

        world = self._modules.get("world")
        if world is not None and goal:
            try:
                if hasattr(world, "observe"):
                    world.observe(str(observation))
                plan = world.plan([f"action_{i}" for i in range(3)])
                trace["stages"]["plan"] = {
                    "best": plan.get("best_action"),
                    "σ": float(plan.get("best_σ", 0.5)),
                }
            except Exception:
                trace["stages"]["plan"] = {"best": None, "σ": 0.5}

        conscious = self._modules.get("conscious")
        if conscious is not None:
            try:
                fn = getattr(conscious, "metacognitive_vector", None)
                if callable(fn):
                    meta = fn(str(observation), str(goal or ""))
                else:
                    meta = conscious.predict_own_σ(str(observation), str(goal or ""))
                trace["stages"]["metacognition"] = {
                    "σ_meta": float(meta.get("σ_meta", 0.5)),
                    "action": str(meta.get("action", "proceed")),
                }
            except Exception:
                trace["stages"]["metacognition"] = {
                    "σ_meta": 0.5,
                    "action": "proceed",
                }

        drive = self._modules.get("drive")
        if drive is not None:
            try:
                drive.record(float(σ_perceive))
                emotion = drive.emotion()
                trace["stages"]["emotion"] = emotion
            except Exception:
                trace["stages"]["emotion"] = {}

        if verdict == "ABSTAIN":
            trace["stages"]["act"] = {
                "action": "ABSTAIN",
                "reason": "σ too high",
                "σ": round(float(σ_perceive), 4),
            }
        else:
            trace["stages"]["act"] = {
                "action": "EXECUTE",
                "σ": round(float(σ_perceive), 4),
            }

        self.σ_trace.append(float(σ_perceive))

        ttt = self._modules.get("ttt")
        if ttt is not None and float(σ_perceive) > 0.4:
            try:
                ttt_result = ttt.process(str(observation), str(goal or ""), None)
                trace["stages"]["learn"] = {
                    "adapted": bool(ttt_result.get("adapted", False)),
                }
            except Exception:
                trace["stages"]["learn"] = {"adapted": False}
        else:
            trace["stages"]["learn"] = {
                "adapted": False,
                "reason": "σ low enough or TTT unavailable",
            }

        conv = self._modules.get("convergence")
        if conv is not None:
            try:
                conv.record(float(σ_perceive))
                check = conv.check()
                trace["stages"]["convergence"] = check
                st = str(check.get("status", ""))
                if st in ("LOOP", "OSCILLATING"):
                    trace["stages"]["act"] = {
                        "action": "HALT",
                        "reason": st,
                        "σ": round(float(σ_perceive), 4),
                    }
            except Exception:
                trace["stages"]["convergence"] = {"status": "CONTINUE", "error": True}

        sigmas: List[float] = []
        for row in trace["stages"].values():
            if isinstance(row, dict):
                s = self._extract_σ(row)
                if s is not None:
                    sigmas.append(s)
        trace["σ_cycle"] = round(sum(sigmas) / max(len(sigmas), 1), 4)

        return trace

    def dream(self) -> Dict[str, Any]:
        """Offline hooks: world replay / prune; engram session end (best-effort)."""
        results: Dict[str, Any] = {}
        world = self._modules.get("world")
        if world is not None:
            try:
                results["world"] = world.dream()
            except Exception:
                results["world"] = {"dreamed": False, "error": True}

        engram = self._modules.get("engram")
        if engram is not None:
            try:
                closer = getattr(engram, "close_session", None)
                if callable(closer):
                    closer()
                    results["engram"] = {"consolidated": True}
                else:
                    ender = getattr(engram, "end_session", None)
                    if callable(ender):
                        ender("mega dream consolidation")
                    results["engram"] = {"consolidated": True}
            except Exception:
                results["engram"] = {"consolidated": False}
        return results

    def status(self) -> Dict[str, Any]:
        """Module availability + short σ telemetry."""
        loaded = {name: mod is not None for name, mod in self._modules.items()}
        window = self.σ_trace[-50:]
        avg = sum(window) / max(len(window), 1) if window else 0.5
        trend = "stable_or_degrading"
        if len(self.σ_trace) > 5 and self.σ_trace[-1] < self.σ_trace[-5]:
            trend = "improving"
        return {
            "cycle": self.cycle,
            "modules_loaded": sum(1 for v in loaded.values() if v),
            "modules_total": len(loaded),
            "modules": loaded,
            "avg_σ": round(float(avg), 4),
            "σ_trend": trend,
        }

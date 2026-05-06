# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Executable Ω cognitive step — perceive → gate → act with σ-history (lab wiring).

Pairs :class:`SigmaGate`, :class:`~cos.memory.SigmaMemory`, and :class:`~cos.graph.SigmaGraph`.
For the 14-lane phase harness see :class:`OmegaPhaseHarness` in ``loop.py``.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Union

__all__ = ["OmegaLoop"]


Input = Union[str, Dict[str, Any]]


class OmegaLoop:
    """Ω = argmin ∫σ(t)dt — discrete lab proxy via average step σ in :meth:`total_σ`."""

    def __init__(
        self,
        gate: Optional[Any] = None,
        memory: Optional[Any] = None,
        graph: Optional[Any] = None,
        config: Optional[Any] = None,
        *,
        world_model: Optional[Any] = None,
    ) -> None:
        from cos.config import SigmaConfig
        from cos.graph import SigmaGraph
        from cos.memory import SigmaMemory
        from cos.sigma_gate import SigmaGate

        g = gate if gate is not None else SigmaGate()
        self.gate = g
        self.memory = memory if memory is not None else SigmaMemory(gate=g)
        self.graph = graph if graph is not None else SigmaGraph(gate=g)
        self.config = config if config is not None else SigmaConfig()
        self.world_model = world_model
        self.σ_history: List[Dict[str, Any]] = []
        self._last_jepa: Optional[Dict[str, Any]] = None

    @staticmethod
    def _verdict_token(verdict: Any) -> str:
        s = str(verdict).upper()
        if "ABSTAIN" in s:
            return "ABSTAIN"
        if "RETHINK" in s:
            return "RETHINK"
        if "ACCEPT" in s:
            return "ACCEPT"
        return str(verdict)

    def _perceive(self, input_data: Input) -> str:
        if isinstance(input_data, dict):
            parts = []
            for k in ("text", "goal", "prompt", "input"):
                if k in input_data and input_data[k]:
                    parts.append(str(input_data[k]))
            perceived = "\n".join(parts) if parts else str(input_data)
        else:
            perceived = str(input_data).strip()
        return perceived or "∅"

    def _predict_baseline(self, perceived: str, context: str) -> str:
        glimpses: List[str] = []
        try:
            tokens = [t for t in perceived.replace(",", " ").split() if len(t) > 2][:3]
            for t in tokens:
                for rel in ("related_to", "mentions", "is_a"):
                    try:
                        trips = self.graph.query(t, relation=rel)
                    except Exception:
                        trips = []
                    for tr in trips[:2]:
                        glimpses.append(f"{tr.subject} {tr.relation} {tr.object}")
        except Exception:
            pass
        base = f"next: elaborate '{perceived[:120]}'"
        if glimpses:
            base += " | graph: " + "; ".join(glimpses[:4])
        if context:
            base += " | mem: " + context[:200]
        return base

    def _predict(self, perceived: str, context: str) -> str:
        base = self._predict_baseline(perceived, context)
        wm = self.world_model
        self._last_jepa = None
        if wm is None:
            return base
        try:
            wm_out = wm.step(perceived)
            self._last_jepa = dict(wm_out)
            jepa_line = (
                f"jepa_wm: σ_pred={float(wm_out['σ']):.4f} verdict={wm_out['verdict']} "
                f"surprise={wm_out['surprise']}"
            )
            return f"{jepa_line} | {base}"
        except Exception:
            return base

    def _remember(self, perceived: str) -> str:
        rows: List[Any] = []
        try:
            rows = self.memory.recall(perceived, top_k=5)
        except Exception:
            rows = []
        if not rows:
            return ""
        lines = [str(r.get("content", r)) for r in rows if r]
        return "\n".join(lines)[:4000]

    def _think(self, perceived: str, context: str, prediction: str, *, rethink: bool = False) -> str:
        tag = " [rethink]" if rethink else ""
        ctx = f"Context:\n{context}\n\n" if context else ""
        pred = f"Prediction:\n{prediction}\n\n" if prediction else ""
        return f"{ctx}{pred}Reasoning: integrate '{perceived[:800]}' under σ-gate policy.{tag}"

    def _act(self, action: Dict[str, Any]) -> Dict[str, Any]:
        t = action.get("type")
        if t == "abstain":
            return {"ok": False, "kind": "abstain", "detail": action.get("reason", "σ")}
        if t == "rethink":
            return {"ok": True, "kind": "rethink", "reasoning": action.get("result")}
        if t == "accept":
            return {"ok": True, "kind": "accept", "reasoning": action.get("result")}
        return {"ok": False, "kind": "unknown", "detail": action}

    def _learn(self, perceived: str, reasoning: str, σ: float, verdict: str) -> None:
        try:
            blob = f"Ω-step | v={verdict} | σ={σ:.4f} | in={perceived[:200]} | out={str(reasoning)[:400]}"
            self.memory.write(blob, memory_type="episodic", source="omega_loop", sigma=float(σ), force=True)
        except Exception:
            pass
        try:
            summary = f"{perceived[:80]} entails summary line"
            self.graph.add(perceived[:48], "omega_summarized", summary[:48], sigma=float(σ))
        except Exception:
            pass

    def _reflect(self, σ: float, verdict: Any, result: Dict[str, Any]) -> float:
        vn = self._verdict_token(verdict)
        penalty = 0.15 if vn == "ABSTAIN" else 0.0
        if not result.get("ok", False):
            penalty += 0.1
        return max(0.0, min(1.0, float(σ) * 0.85 + penalty))

    def step(self, input_data: Input) -> Dict[str, Any]:
        """One Ω-step: perceive → remember → predict → think → σ-gate → decide → act → learn → reflect."""
        perceived = self._perceive(input_data)
        context = self._remember(perceived)
        prediction = self._predict(perceived, context)
        reasoning = self._think(perceived, context, prediction, rethink=False)
        σ, verdict = self.gate.score(str(perceived), str(reasoning))
        vn = self._verdict_token(verdict)

        if vn == "ABSTAIN":
            action: Dict[str, Any] = {"type": "abstain", "reason": "σ too high"}
        elif vn == "RETHINK":
            reasoning = self._think(perceived, context, prediction, rethink=True)
            σ, verdict = self.gate.score(str(perceived), str(reasoning))
            vn = self._verdict_token(verdict)
            if vn == "ABSTAIN":
                action = {"type": "abstain", "reason": "σ too high after rethink"}
            elif vn == "ACCEPT":
                action = {"type": "accept", "result": reasoning}
            else:
                action = {"type": "rethink", "result": reasoning}
        else:
            action = {"type": "accept", "result": reasoning}

        result = self._act(action)
        self._learn(perceived, reasoning, σ, str(vn))
        σ_meta = self._reflect(σ, vn, result)
        self.σ_history.append(
            {
                "σ": float(σ),
                "σ_meta": float(σ_meta),
                "verdict": str(vn),
                "input": str(input_data)[:100],
            }
        )
        return {
            "result": result,
            "σ": float(σ),
            "σ_meta": float(σ_meta),
            "verdict": str(vn),
            "step": len(self.σ_history),
        }

    def run(self, inputs: Sequence[Input], max_steps: Optional[int] = None) -> List[Dict[str, Any]]:
        """Run :meth:`step` over ``inputs`` (optional cap)."""
        results: List[Dict[str, Any]] = []
        lim = len(inputs) if max_steps is None else min(len(inputs), int(max_steps))
        for i, inp in enumerate(inputs):
            if i >= lim:
                break
            results.append(self.step(inp))
        return results

    def total_σ(self) -> float:
        """Average historical σ (discrete ∫σ dt proxy)."""
        if not self.σ_history:
            return 0.0
        return sum(float(h["σ"]) for h in self.σ_history) / len(self.σ_history)

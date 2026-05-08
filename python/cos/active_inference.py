# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Active inference scaffold: Ω-style **perceive → predict → act → update** with σ scoring.

In Creation OS lab notation, σ is a **scalar coherence / prediction-error proxy** (the deployed
:class:`~cos.sigma_gate.SigmaGate`), not a literal variational free energy bound. The *expected*
σ objective (ambiguity + risk blend) is a **pedagogical discrete analogy** to expected free
energy, for routing and tracing — **not** a neuroscience claim and **not** “AGI achieved”.

The loop encourages **action selection** that reduces anticipated mismatch against preferences
when a world model is absent or present. Optional :meth:`act_factor_graph` routes the same
choice through a tiny σ-factor graph (VFE proxy). See corpus paper #86 for the theoretical mapping;
this module is an explicit, testable harness.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, List

from cos.sigma_gate import SigmaGate

__all__ = ["ActiveInference"]


class ActiveInference:
    """Perceive–predict–act–update loop scored by σ (lab stand-in for prediction error )."""

    def __init__(self, gate: Any = None, world_model: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.world_model = world_model
        self.beliefs: Dict[str, Any] = {}
        self.preferences: Dict[str, Any] = {}
        self.sigma_history: List[Dict[str, Any]] = []
        self.action_history: List[Dict[str, Any]] = []

    @staticmethod
    def _sigma_from(val: Any, default: float = 0.5) -> float:
        """Extract σ from prediction-like dicts (supports ``sigma`` / ``σ``)."""
        if isinstance(val, dict):
            if "σ" in val:
                return float(val["σ"])
            if "sigma" in val:
                return float(val["sigma"])
        return float(default)

    def _append_sigma(self, kind: str, sigma: float, **extra: Any) -> None:
        s = float(sigma)
        entry: Dict[str, Any] = {"type": kind, "sigma": s, "σ": round(s, 4), **extra}
        self.sigma_history.append(entry)

    def set_preference(self, key: str, desired_state: Any) -> None:
        """Register a preferred outcome label/value for risk scoring."""
        self.preferences[str(key)] = desired_state

    def perceive(self, observation: Any) -> Dict[str, Any]:
        """Update beliefs; σ = gate(beliefs snapshot vs observation)."""
        sigma, verdict = self.gate.score(str(self.beliefs), str(observation))
        s = float(sigma)
        self.beliefs["last_observation"] = observation
        self.beliefs["σ_perception"] = s
        self.beliefs["sigma_perception"] = s
        self._append_sigma("perceive", s)
        return {"σ": round(s, 4), "sigma": round(s, 4), "verdict": str(verdict)}

    def predict(self, action: Any = None) -> Dict[str, Any]:
        """Predict next observation; optional ``world_model.predict(beliefs, action)``."""
        wm = self.world_model
        if wm is not None and hasattr(wm, "predict"):
            try:
                raw = wm.predict(str(self.beliefs), str(action))
                if isinstance(raw, dict):
                    pred = dict(raw)
                else:
                    pred = {"predicted": raw}
            except Exception:  # noqa: BLE001
                pred = {"predicted": self.beliefs.get("last_observation", ""), "σ": 0.5}
        else:
            pred = {
                "predicted": self.beliefs.get("last_observation", ""),
                "σ": 0.5,
                "sigma": 0.5,
            }
        if "σ" not in pred and "sigma" in pred:
            pred["σ"] = float(pred["sigma"])
        if "sigma" not in pred and "σ" in pred:
            pred["sigma"] = float(pred["σ"])
        return pred

    def evaluate_actions(self, candidate_actions: List[Any]) -> List[Dict[str, Any]]:
        """Rank actions by blended expected σ (ambiguity + risk vs preferences)."""
        scored: List[Dict[str, Any]] = []
        for action in candidate_actions:
            prediction = self.predict(action)
            σ_ambiguity = self._sigma_from(prediction, 0.5)
            σ_risk = 0.5
            if self.preferences:
                pref_str = str(list(self.preferences.values()))
                pred_str = str(prediction.get("predicted", ""))
                σ_risk, _ = self.gate.score(pref_str, pred_str)
                σ_risk = float(σ_risk)
            expected_σ = (σ_ambiguity + σ_risk) / 2.0
            scored.append(
                {
                    "action": action,
                    "σ_ambiguity": round(σ_ambiguity, 4),
                    "σ_risk": round(σ_risk, 4),
                    "expected_σ": round(expected_σ, 4),
                }
            )
        scored.sort(key=lambda x: x["expected_σ"])
        return scored

    def act(self, candidate_actions: List[Any]) -> Dict[str, Any]:
        """Pick action with lowest expected σ; stash ``last_prediction`` for :meth:`update`."""
        evaluated = self.evaluate_actions(candidate_actions)
        if not evaluated:
            return {
                "chosen_action": None,
                "action": None,
                "expected_σ": 1.0,
                "σ": 1.0,
                "sigma": 1.0,
                "reason": "no actions",
                "alternatives": 0,
            }

        best = evaluated[0]
        pred = self.predict(best["action"])
        self.beliefs["last_prediction"] = pred.get("predicted", pred)

        self.action_history.append(best)
        ex = float(best["expected_σ"])
        self._append_sigma("act", ex, action=best["action"])

        return {
            "chosen_action": best["action"],
            "action": best["action"],
            "expected_σ": ex,
            "σ": round(ex, 4),
            "sigma": round(ex, 4),
            "alternatives": len(evaluated) - 1,
            "reason": f"minimizes expected σ ({ex:.3f})",
        }

    def act_factor_graph(self, candidate_actions: List[Any]) -> Dict[str, Any]:
        """Pick action via :class:`~cos.factor_graph.SigmaFactorGraph.active_infer` (VFE proxy)."""
        from cos.factor_graph import SigmaFactorGraph

        if not candidate_actions:
            return {
                "chosen_action": None,
                "action": None,
                "expected_σ": 1.0,
                "expected_vfe": 1.0,
                "σ": 1.0,
                "sigma": 1.0,
                "reason": "no actions",
                "alternatives": 0,
                "factor_graph": None,
            }

        fg = SigmaFactorGraph(gate=self.gate)
        fg.add_variable("observation", str(self.beliefs.get("last_observation", "")))
        fg.add_variable("action", "")
        fg.add_factor("policy", ["observation", "action"])
        raw = fg.active_infer([str(a) for a in candidate_actions], "action", infer_iter=8)

        best_action = raw.get("best_action")
        best_vfe = float(raw.get("best_vfe", 1.0))

        pred: Dict[str, Any]
        if best_action is not None:
            pred = self.predict(best_action)
            self.beliefs["last_prediction"] = pred.get("predicted", pred)
        else:
            pred = {"predicted": "", "σ": 0.5, "sigma": 0.5}

        self.beliefs["last_factor_graph"] = raw
        self._append_sigma("act_factor_graph", best_vfe, action=best_action)

        return {
            "chosen_action": best_action,
            "action": best_action,
            "expected_vfe": round(best_vfe, 4),
            "expected_σ": round(best_vfe, 4),
            "σ": round(best_vfe, 4),
            "sigma": round(best_vfe, 4),
            "alternatives": len(candidate_actions) - 1,
            "reason": f"minimizes factor-graph VFE proxy ({best_vfe:.3f})",
            "factor_graph": raw,
            "prediction": pred,
        }

    def update(self, observation_after_action: Any) -> Dict[str, Any]:
        """Close loop: compare stored prediction vs new observation."""
        prediction = self.beliefs.get("last_prediction", "")
        sigma, verdict = self.gate.score(str(prediction), str(observation_after_action))
        s = float(sigma)
        prior = float(self.beliefs.get("σ_perception", self.beliefs.get("sigma_perception", 1.0)))
        self.beliefs["last_observation"] = observation_after_action
        self.beliefs["prediction_error"] = s
        self._append_sigma("update", s)

        return {
            "prediction_error": round(s, 4),
            "σ": round(s, 4),
            "sigma": round(s, 4),
            "verdict": str(verdict),
            "belief_updated": True,
            "model_improving": s < prior,
        }

    def loop(
        self,
        observations: List[Any],
        candidate_actions_fn: Callable[[Any], List[Any]],
        n_steps: int = 10,
    ) -> Dict[str, Any]:
        """Run multiple perceive → act → update cycles (same observation used as feedback lab)."""
        results: List[Dict[str, Any]] = []
        lim = min(len(observations), max(1, int(n_steps)))
        for i in range(lim):
            obs = observations[i]
            p = self.perceive(obs)
            actions = candidate_actions_fn(obs)
            a = self.act(actions)
            u = self.update(obs)
            results.append(
                {
                    "step": i,
                    "observation": str(obs)[:50],
                    "perception_σ": p["σ"],
                    "action": a.get("chosen_action"),
                    "action_σ": a.get("expected_σ"),
                    "update_σ": u["prediction_error"],
                }
            )

        perception_vals = [r["perception_σ"] for r in results]
        total = (
            round(sum(perception_vals) / max(len(perception_vals), 1), 4)
            if perception_vals
            else 0.5
        )
        trend = "stable_or_degrading"
        if len(results) > 1 and results[-1]["perception_σ"] < results[0]["perception_σ"]:
            trend = "improving"

        return {
            "steps": results,
            "total_σ": total,
            "sigma_trend": trend,
            # Alias for sketch / JSON consumers
            "σ_trend": trend,
        }

    def free_energy(self) -> float:
        """Rolling mean σ over recent history (discrete proxy, not VFE)."""
        if not self.sigma_history:
            return 0.5
        tail = self.sigma_history[-10:]
        return round(sum(float(h["sigma"]) for h in tail) / len(tail), 4)

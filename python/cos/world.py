# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.

"""σ-world — lightweight world-model lab (commonsense check, action prediction, rollout).

Uses an optional :class:`~cos.graph.SigmaGraph` for **heuristic** grounding. This is **not**
a physics engine or AGI world model. See ``docs/CLAIM_DISCIPLINE.md``.

**V2 — σ-validated imagination:** :class:`SigmaWorldV2` simulates action rollouts with per-step
σ scoring, planning by lowest simulated stress, counterfactuals, and offline ``dream`` pruning.
**NOT AGI ACHIEVED** — lab scaffold only."""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaWorld", "SigmaWorldV2", "WorldState"]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1] if "." in raw else raw


def _verdict_norm(verdict: Any) -> str:
    return str(_verdict_str(verdict)).strip().upper()


class WorldState:
    """One recorded observation in the running state list."""

    def __init__(
        self,
        description: str,
        features: Optional[Dict[str, Any]] = None,
        timestamp: Optional[float] = None,
    ) -> None:
        self.description = str(description)
        self.features = dict(features or {})
        self.timestamp = float(timestamp if timestamp is not None else time.time())

    def __repr__(self) -> str:
        return f"State({self.description[:40]!r})"


class SigmaWorld:
    """Observe → forecast → one-step transition (gate); optional graph for commonsense."""

    def __init__(
        self,
        gate: Any = None,
        max_history: int = 100,
        *,
        graph: Any = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.graph = graph
        self.states: List[WorldState] = []
        self.max_history = int(max_history)
        self.predictions: List[Dict[str, Any]] = []

    def observe(
        self,
        description: str,
        features: Optional[Dict[str, Any]] = None,
    ) -> WorldState:
        state = WorldState(description, features)
        self.states.append(state)
        if len(self.states) > self.max_history:
            self.states = self.states[-self.max_history :]
        self._validate_predictions(description)
        return state

    def forecast(self, query: str, n_steps: int = 1) -> Dict[str, Any]:
        """Heuristic forecast from recent state text + feature drift (legacy path)."""
        del n_steps
        if not self.states:
            return {"prediction": None, "confidence": 0.0, "basis": "no history", "query": query}

        recent = self.states[-10:]
        feature_trend = self._analyze_feature_trend()

        prediction: Dict[str, Any] = {
            "query": query,
            "basis_states": len(recent),
            "feature_trend": feature_trend,
            "timestamp": time.time(),
        }
        self.predictions.append(prediction)
        return prediction

    def transition(self, action: str, current_state: Optional[WorldState] = None) -> Dict[str, Any]:
        """Score a single action against the latest (or given) :class:`WorldState`."""
        if current_state is None:
            current_state = self.states[-1] if self.states else None

        if current_state is None:
            return {"outcome": None, "sigma": 1.0, "verdict": "ABSTAIN"}

        scenario = f"Given state: {current_state.description}. Action: {action}"
        sigma, verdict = self.gate.score(scenario, action)
        return {
            "action": action,
            "current_state": current_state.description,
            "sigma": round(float(sigma), 4),
            "verdict": str(verdict),
            "n_states_considered": len(self.states),
        }

    def plan(self, goal: str, max_steps: int = 5) -> Dict[str, Any]:
        steps: List[Dict[str, Any]] = []
        for i in range(max_steps):
            step_desc = f"Step {i + 1} toward: {goal}"
            sim = self.transition(step_desc)
            steps.append(
                {
                    "step": i + 1,
                    "action": step_desc,
                    "sigma": sim["sigma"],
                    "verdict": sim["verdict"],
                }
            )
            if sim["verdict"] == "ACCEPT":
                break
            if sim["verdict"] == "ABSTAIN":
                break

        total_sigma = sum(float(s["sigma"]) for s in steps) / max(len(steps), 1)
        return {
            "goal": goal,
            "steps": steps,
            "total_sigma": round(total_sigma, 4),
            "feasible": total_sigma < 0.5,
            "n_steps": len(steps),
        }

    def commonsense_check(self, statement: str, context: str = "") -> Dict[str, Any]:
        """Graph-grounded sketch + gate fallback; high σ on a stored triple vs claim → implausible."""
        stmt = str(statement).strip()
        ctx = str(context).strip()

        g = self.graph
        if g is not None and hasattr(g, "entities") and callable(g.entities):
            words = stmt.lower().split()
            entities_list = list(g.entities())
            entities = [e for e in entities_list if e.lower() in stmt.lower()]
            if entities and hasattr(g, "relations_of"):
                for entity in entities:
                    rels = g.relations_of(entity)
                    for rel in rels:
                        contradiction = f"{rel['subject']} {rel['relation']} {rel['object']}"
                        σ_check, _ = self.gate.score(contradiction, stmt)
                        if float(σ_check) > 0.7:
                            return {
                                "plausible": False,
                                "σ": round(float(σ_check), 4),
                                "sigma": round(float(σ_check), 4),
                                "contradiction": contradiction,
                                "verdict": "RETHINK",
                            }

        σ, verdict = self.gate.score(ctx or "common sense", stmt)
        vn = _verdict_str(verdict)
        sf = round(float(σ), 4)
        return {
            "plausible": vn != "ABSTAIN",
            "σ": sf,
            "sigma": sf,
            "verdict": vn,
        }

    def predict(self, current_state: str, action: str) -> Dict[str, Any]:
        """Predict outcome of *action* in *current_state* (template response + σ)."""
        cs, act = str(current_state).strip(), str(action).strip()
        prompt = f"State: {cs}. Action: {act}. What happens?"
        response = f"After {act}, the state changes"
        σ, verdict = self.gate.score(prompt, response)
        vn = _verdict_str(verdict)
        return {
            "predicted_outcome": f"Effect of {act} on {cs}",
            "σ": round(float(σ), 4),
            "confidence": round(1.0 - float(σ), 4),
            "verdict": vn,
        }

    def simulate(self, initial_state: str, actions: Sequence[str]) -> Dict[str, Any]:
        """Roll out a list of actions; accumulate σ along the trajectory."""
        state = str(initial_state).strip()
        trajectory: List[Dict[str, Any]] = []
        cumulative_σ = 0.0
        acts = list(actions)

        for action in acts:
            result = self.predict(state, str(action))
            sg = float(result["σ"])
            cumulative_σ += sg
            trajectory.append(
                {
                    "state": state,
                    "action": str(action),
                    "σ": sg,
                    "cumulative_σ": round(cumulative_σ, 4),
                }
            )
            state = str(result["predicted_outcome"])

        return {
            "trajectory": trajectory,
            "final_σ": round(cumulative_σ / max(len(acts), 1), 4),
            "steps": len(acts),
        }

    def _validate_predictions(self, actual: str) -> None:
        for pred in self.predictions:
            if "validated" not in pred:
                pred["validated"] = True
                pred["actual"] = actual[:50]

    def _analyze_feature_trend(self) -> Dict[str, str]:
        if len(self.states) < 2:
            return {}
        recent = self.states[-5:]
        all_keys: set[str] = set()
        for s in recent:
            all_keys.update(s.features.keys())
        trends: Dict[str, str] = {}
        for key in all_keys:
            values = [s.features.get(key) for s in recent if key in s.features]
            if len(values) >= 2 and all(isinstance(v, (int, float)) for v in values):
                trend = float(values[-1]) - float(values[0])
                if trend > 0:
                    trends[key] = "rising"
                elif trend < 0:
                    trends[key] = "falling"
                else:
                    trends[key] = "stable"
        return trends

    def predict_next_state(self, hint: str = "") -> Dict[str, Any]:
        if not self.states:
            return {"predicted_description": None, "sigma": 1.0, "verdict": "ABSTAIN", "feature_trend": {}}
        tr = self._analyze_feature_trend()
        last = self.states[-1]
        chunks = [last.description]
        for key, direction in tr.items():
            fv = last.features.get(key)
            if isinstance(fv, (int, float)):
                step = 0.1 if direction == "rising" else -0.1 if direction == "falling" else 0.0
                chunks.append(f"{key} ~{fv + step} ({direction})")
        desc = "; ".join(chunks) + (f" | {hint}" if hint else "")
        sigma, verdict = self.gate.score("predict_next_world_state", desc[:1500])
        return {
            "predicted_description": desc,
            "sigma": round(float(sigma), 4),
            "verdict": str(verdict),
            "feature_trend": tr,
        }


class SigmaWorldV2:
    """σ-validated imagination lab: roll out futures, plan by lowest simulated σ, dream-prune.

    **NOT AGI ACHIEVED** — no real physics or robotic transfer; see ``docs/CLAIM_DISCIPLINE.md``."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.state: Dict[str, Any] = {}
        self.dynamics: List[Dict[str, Any]] = []
        self.simulations: List[Dict[str, Any]] = []

    def observe(self, observation: Any) -> Dict[str, Any]:
        """Update running dict state; score observation against prior state string."""
        σ, verdict = self.gate.score(str(self.state), str(observation))
        self.state["last"] = observation
        self.state["σ"] = round(float(σ), 4)
        self.state["verdict_last"] = _verdict_norm(verdict)
        return {"σ": round(float(σ), 4), "verdict": _verdict_norm(verdict), "state_updated": True}

    def learn_dynamics(self, cause: Any, effect: Any) -> Dict[str, Any]:
        """Record a cause→effect pair; σ marks whether the pair looks reliable."""
        σ, verdict = self.gate.score(str(cause), str(effect))
        sg = round(float(σ), 4)
        row = {
            "cause": cause,
            "effect": effect,
            "σ": sg,
            "reliable": sg < 0.3,
            "verdict": _verdict_norm(verdict),
        }
        self.dynamics.append(row)
        return {"learned": True, "σ": sg, "reliable": row["reliable"]}

    def simulate(self, action: Any, n_steps: int = 5) -> Dict[str, Any]:
        """Roll forward from ``state['last']`` using learned dynamics; σ per step."""
        trajectory: List[Dict[str, Any]] = []
        current = str(self.state.get("last", ""))
        cumulative_σ = 0.0
        scored_steps = 0

        for step in range(int(n_steps)):
            predicted = self._predict_next(current, action)
            σ, verdict = self.gate.score(f"step {step}: {current}", str(predicted))
            sg = float(σ)
            cumulative_σ += sg
            scored_steps += 1
            vn = _verdict_norm(verdict)

            trajectory.append(
                {
                    "step": step,
                    "state": str(predicted)[:100],
                    "σ": round(sg, 4),
                    "verdict": vn,
                    "cumulative_σ": round(cumulative_σ, 4),
                }
            )

            if vn == "ABSTAIN":
                trajectory.append(
                    {
                        "step": step + 1,
                        "HORIZON_LIMIT": True,
                        "reason": "σ-gate ABSTAIN — stop imagined rollout",
                    }
                )
                break

            current = str(predicted)

        avg_σ = cumulative_σ / max(scored_steps, 1)
        rec = {
            "action": action,
            "trajectory": trajectory,
            "avg_σ": round(float(avg_σ), 4),
        }
        self.simulations.append(rec)

        return {
            "action": action,
            "trajectory": trajectory,
            "horizon": len([t for t in trajectory if "σ" in t]),
            "avg_σ": round(float(avg_σ), 4),
            "reliable": float(avg_σ) < 0.4,
        }

    def plan(self, candidate_actions: Sequence[Any], n_steps: int = 5) -> Dict[str, Any]:
        """Simulate each candidate; prefer lowest average σ."""
        plans: List[Dict[str, Any]] = []
        for act in candidate_actions:
            sim = self.simulate(act, n_steps)
            plans.append(
                {
                    "action": act,
                    "avg_σ": sim["avg_σ"],
                    "horizon": sim["horizon"],
                    "reliable": sim["reliable"],
                }
            )

        plans.sort(key=lambda p: float(p["avg_σ"]))
        best = plans[0] if plans else None

        return {
            "best_action": best["action"] if best else None,
            "best_σ": float(best["avg_σ"]) if best else 1.0,
            "alternatives": max(len(plans) - 1, 0),
            "all_plans": plans,
        }

    def counterfactual(self, action_taken: Any, action_alternative: Any) -> Dict[str, Any]:
        """Compare imagined futures for two actions."""
        sim_taken = self.simulate(action_taken, n_steps=3)
        sim_alt = self.simulate(action_alternative, n_steps=3)
        at = float(sim_taken["avg_σ"])
        aa = float(sim_alt["avg_σ"])
        return {
            "action_taken": action_taken,
            "σ_taken": sim_taken["avg_σ"],
            "action_alternative": action_alternative,
            "σ_alternative": sim_alt["avg_σ"],
            "regret": round(max(0.0, at - aa), 4),
            "better_alternative": aa < at,
        }

    def dream(self) -> Dict[str, Any]:
        """Replay recent simulations; count low/high stress; prune wild dynamics."""
        if not self.simulations:
            return {"dreamed": False, "reason": "no simulations to replay"}

        consolidated = 0
        forgotten = 0

        for sim in self.simulations[-20:]:
            aσ = float(sim.get("avg_σ", 1.0))
            if aσ < 0.3:
                consolidated += 1
            elif aσ > 0.7:
                forgotten += 1

        before = len(self.dynamics)
        self.dynamics = [d for d in self.dynamics if float(d.get("σ", 1.0)) < 0.7]
        pruned = before - len(self.dynamics)

        return {
            "dreamed": True,
            "consolidated": consolidated,
            "forgotten": forgotten,
            "dynamics_pruned": pruned,
            "dynamics_remaining": len(self.dynamics),
        }

    def _predict_next(self, current: str, action: Any) -> str:
        """Pick best matching reliable effect; fallback template."""
        best_match: Optional[str] = None
        best_σ = 1.0

        for d in self.dynamics:
            if not d.get("reliable"):
                continue
            σ, _ = self.gate.score(f"{current} + {action}", str(d.get("effect", "")))
            if float(σ) < best_σ:
                best_σ = float(σ)
                best_match = str(d.get("effect", ""))

        return best_match if best_match is not None else f"predicted({current}, {action})"

    def imagination_budget(self) -> Dict[str, Any]:
        """Rough capacity signal: fraction of dynamics marked reliable."""
        reliable = sum(1 for d in self.dynamics if d.get("reliable"))
        n = len(self.dynamics)
        return {
            "dynamics_total": n,
            "dynamics_reliable": reliable,
            "imagination_power": round(reliable / max(n, 1), 4),
            "simulations_run": len(self.simulations),
        }

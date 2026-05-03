# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-workflow — agentic step orchestration with σ-gate as verification (lab).

Checkpoints are in-memory by default; pair with :class:`cos.snapshot.SnapshotManager` for disk.
See ``docs/CLAIM_DISCIPLINE.md`` (no production SLA claims)."""
from __future__ import annotations

import concurrent.futures
import time
from typing import Any, Callable, Dict, List, Mapping, Optional, Sequence, Tuple

from cos.sigma_gate import ABSTAIN, ACCEPT, RETHINK, SigmaGate
from cos.trace import SigmaTrace

__all__ = ["SigmaWorkflow"]

HumanGateFn = Optional[Callable[[Mapping[str, Any], float, str], bool]]
StepMap = Mapping[str, Any]


class SigmaWorkflow:
    """Define steps with σ thresholds, retries, human gate, fallbacks; trace + optional metrics."""

    def __init__(
        self,
        gate: Any = None,
        *,
        human_gate_fn: HumanGateFn = None,
        metrics: Any = None,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.human_gate_fn = human_gate_fn
        self.metrics = metrics
        self._checkpoints: List[Dict[str, Any]] = []
        self.cost_eur_total = 0.0

    def define(self, steps: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Normalize step specs: ``name``, ``fn``, ``sigma_threshold``, ``checkpoint``, …"""
        norm: List[Dict[str, Any]] = []
        for s in steps:
            norm.append(
                {
                    "name": str(s["name"]),
                    "fn": s["fn"],
                    "sigma_threshold": float(s.get("sigma_threshold", 1.0)),
                    "checkpoint": bool(s.get("checkpoint", False)),
                    "human_gate": bool(s.get("human_gate", False)),
                    "fallback": s.get("fallback"),
                    "max_retries": int(s.get("max_retries", 1)),
                    "step_cost_eur": float(s.get("step_cost_eur", 0.0)),
                }
            )
        return {"steps": norm, "version": 1}

    def checkpoint(self, step: StepMap, state: Dict[str, Any]) -> Dict[str, Any]:
        snap = {"step_name": step.get("name"), "state": dict(state), "ts": time.time()}
        self._checkpoints.append(snap)
        return {"checkpoint_index": len(self._checkpoints) - 1, **snap}

    def rollback(self, step: StepMap) -> Dict[str, Any]:
        target = str(step.get("name", ""))
        for i in range(len(self._checkpoints) - 1, -1, -1):
            if self._checkpoints[i].get("step_name") == target:
                return {"ok": True, "state": dict(self._checkpoints[i]["state"]), "index": i}
        return {"ok": False, "error": "no_checkpoint_for_step"}

    @staticmethod
    def timeout(step: Mapping[str, Any], max_seconds: float) -> Dict[str, Any]:
        fn = step["fn"]
        lim = max(0.01, float(max_seconds))

        def wrapped(st: Dict[str, Any]) -> Dict[str, Any]:
            with concurrent.futures.ThreadPoolExecutor(max_workers=1) as ex:
                fut = ex.submit(fn, st)
                return dict(fut.result(timeout=lim))

        out = dict(step)
        out["fn"] = wrapped
        return out

    def human_gate(self, step: StepMap, sigma: float, verdict: str) -> bool:
        if self.human_gate_fn is None:
            return True
        return bool(self.human_gate_fn(step, float(sigma), str(verdict)))

    def parallel(
        self,
        steps: Sequence[Mapping[str, Any]],
        state: Dict[str, Any],
    ) -> Tuple[Dict[str, Any], float, str]:
        """Run branch fns; pick branch with lowest σ on the same score keys."""
        best_state = dict(state)
        best_sigma = 1e9
        best_verdict = ""
        for st in steps:
            ns = dict(st["fn"](dict(state)))
            p = str(ns.get("prompt", state.get("prompt", "")))
            r = str(ns.get("response", ""))
            sig, ver = self.gate.score(p, r)
            if float(sig) < best_sigma:
                best_sigma = float(sig)
                best_verdict = str(ver)
                best_state = ns
        return best_state, best_sigma, best_verdict

    @staticmethod
    def cascade_error_prevention(
        step_sigmas: Sequence[float],
        *,
        limit: float = 0.35,
        min_index: int = 2,
    ) -> Dict[str, Any]:
        """Stop when σ at or after ``min_index`` (0-based) exceeds ``limit`` — compound-risk guard."""
        for i, s in enumerate(step_sigmas):
            if i >= int(min_index) and float(s) > float(limit):
                return {"stop": True, "at_step_index": i, "sigma": float(s)}
        return {"stop": False}

    def execute(
        self,
        workflow: Mapping[str, Any],
        input_data: Any,
        *,
        resume_state: Optional[Dict[str, Any]] = None,
        cascade_limit: float = 0.35,
    ) -> Dict[str, Any]:
        trace = SigmaTrace()
        steps: List[Dict[str, Any]] = list(workflow["steps"])
        state: Dict[str, Any] = (
            dict(resume_state)
            if resume_state
            else {"input": input_data, "prompt": str(input_data), "response": ""}
        )
        self._checkpoints.clear()
        self.cost_eur_total = 0.0
        step_sigmas: List[float] = []
        stopped: Optional[Dict[str, Any]] = None
        step_results: List[Dict[str, Any]] = []

        for step in steps:
            name = str(step["name"])
            guard = self.cascade_error_prevention(step_sigmas, limit=float(cascade_limit))
            if guard["stop"]:
                stopped = {"reason": "cascade_error_prevention", **guard}
                break

            if step.get("checkpoint"):
                self.checkpoint(step, state)

            last_sigma = 0.0
            last_verdict = ""
            step_finished = False

            with trace.span(f"workflow_step:{name}") as sp:
                attempt = 0
                max_retries = int(step["max_retries"])
                while not step_finished:
                    attempt += 1
                    try:
                        state = dict(step["fn"](state))
                    except Exception as e:
                        stopped = {"reason": "exception", "error": str(e), "step": name}
                        step_finished = True
                        last_sigma = 1.0
                        break

                    self.cost_eur_total += float(step["step_cost_eur"])
                    if self.metrics is not None and hasattr(self.metrics, "observe_cost_eur"):
                        self.metrics.observe_cost_eur(float(step["step_cost_eur"]))

                    prompt = str(state.get("prompt", state.get("input", "")))
                    response = str(state.get("response", ""))
                    sigma, verdict = self.gate.score(prompt, response)
                    last_sigma, last_verdict = float(sigma), str(verdict)
                    sp["sigma"] = last_sigma
                    sp["verdict"] = last_verdict

                    if self.metrics is not None:
                        if hasattr(self.metrics, "record"):
                            self.metrics.record("sigma", last_sigma, tags={"step": name})
                        if hasattr(self.metrics, "observe_verdict"):
                            self.metrics.observe_verdict(last_verdict)

                    if last_sigma > float(step["sigma_threshold"]):
                        stopped = {
                            "reason": "sigma_threshold",
                            "step": name,
                            "sigma": last_sigma,
                            "threshold": float(step["sigma_threshold"]),
                        }
                        step_finished = True
                        break

                    vu = last_verdict.upper()
                    if vu == ACCEPT:
                        state["last_sigma"] = last_sigma
                        state["last_verdict"] = last_verdict
                        step_finished = True
                        break
                    if vu == ABSTAIN:
                        fb = step.get("fallback")
                        if callable(fb):
                            state = dict(fb(state))
                            sigma2, v2 = self.gate.score(
                                str(state.get("prompt", "")),
                                str(state.get("response", "")),
                            )
                            last_sigma, last_verdict = float(sigma2), str(v2)
                            if str(v2).upper() == ACCEPT:
                                state["last_sigma"] = last_sigma
                                state["last_verdict"] = last_verdict
                                step_finished = True
                                break
                        if not step_finished:
                            stopped = {"reason": "abstain", "step": name}
                            step_finished = True
                        break
                    if vu == RETHINK:
                        if attempt <= max_retries:
                            continue
                        if step.get("human_gate") and not self.human_gate(step, last_sigma, last_verdict):
                            stopped = {"reason": "human_gate_denied", "step": name}
                            step_finished = True
                            break
                        stopped = {"reason": "rethink_exhausted", "step": name}
                        step_finished = True
                        break

            step_sigmas.append(last_sigma)
            step_results.append(
                {
                    "step": name,
                    "sigma": last_sigma,
                    "verdict": last_verdict,
                }
            )
            if stopped:
                break

        return {
            "ok": stopped is None,
            "state": state,
            "stopped": stopped,
            "trace_id": trace.trace_id,
            "trace_spans": trace.spans,
            "checkpoints": len(self._checkpoints),
            "cost_eur": round(self.cost_eur_total, 8),
            "step_sigmas": step_sigmas,
            "step_results": step_results,
        }

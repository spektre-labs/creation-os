# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-fabric — wiring layer that composes existing ``cos`` modules into one orchestrated path.

This is **integration**, not a new algorithm: ``boot()`` loads what is importable,
``process()`` runs pipeline + optional metacognition, symbolic consistency, calibration,
and conversation history. σ from the gate remains the primary scalar; the trace records
per-stage summaries for audit. For measured claims and the evidence ladder see
``docs/CLAIM_DISCIPLINE.md`` and ``README.md`` (lab vs harness scope).

Layer map (conceptual — not all code paths load in minimal installs)::

  L1  Inference   — SigmaGate, Pipeline, Stream, Probe, SignalCascade
  L2  Cognition   — Reason (FOL), Metacog, Calibrate
  L3  Memory      — SnapshotManager, ConversationHistory
  L4–L9           — agency, learning, protocol, safety, deploy, “consciousness proxy”
                    hooks live in other modules; wire them here as needed.

σ is propagated from the gate; downstream steps may raise σ or change verdict when
they detect conflict or meta-level abstention.
"""
from __future__ import annotations

import tempfile
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from cos.pipeline import Pipeline, PipelineResult


class SigmaTrace:
    """Audit trail: one entry per integration step (layer tag + σ-like scalar + info)."""

    def __init__(self) -> None:
        self.steps: List[Dict[str, Any]] = []
        self.final_verdict: Optional[str] = None
        self.needs_retrieval: bool = False

    def add(self, layer: str, sigma: Any, info: Any) -> None:
        sig_round = round(float(sigma), 4) if isinstance(sigma, (int, float)) else sigma
        self.steps.append({"layer": layer, "sigma": sig_round, "info": info})

    def to_dict(self) -> Dict[str, Any]:
        return {
            "steps": self.steps,
            "final_verdict": self.final_verdict,
            "depth": len(self.steps),
            "needs_retrieval": self.needs_retrieval,
        }

    def __repr__(self) -> str:
        lines = [f"  {s['layer']}: σ={s['sigma']} → {s['info']}" for s in self.steps]
        head = f"SigmaTrace({self.final_verdict!r})"
        if not lines:
            return f"{head}: <empty>"
        return f"{head}:\n" + "\n".join(lines)


class FabricResult:
    """End state of :meth:`SigmaFabric.process` (text + σ + verdict + trace)."""

    __slots__ = ("text", "sigma", "verdict", "trace", "needs_retrieval", "reason")

    def __init__(
        self,
        text: Optional[str],
        sigma: float,
        verdict: str,
        trace: SigmaTrace,
        *,
        needs_retrieval: bool = False,
        reason: Optional[str] = None,
    ) -> None:
        self.text = text
        self.sigma = float(sigma)
        self.verdict = str(verdict)
        self.trace = trace
        self.needs_retrieval = bool(needs_retrieval)
        self.reason = reason

    def __bool__(self) -> bool:
        return self.verdict == "ACCEPT"

    def __repr__(self) -> str:
        d = self.trace.to_dict()["depth"]
        return f"FabricResult(σ={self.sigma:.4f}, {self.verdict!r}, depth={d})"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "text": self.text,
            "sigma": self.sigma,
            "verdict": self.verdict,
            "reason": self.reason,
            "trace": self.trace.to_dict(),
            "needs_retrieval": self.needs_retrieval,
        }


class SigmaFabric:
    """Boot lightweight L1–L3 objects and run :meth:`process` with a single σ trace."""

    def __init__(self, *, snapshot_dir: Optional[Union[str, Path]] = None) -> None:
        self.layers: Dict[str, Any] = {}
        self.booted = False
        self.sigma_bus: List[float] = []
        self._snapshot_dir = snapshot_dir

    def boot(self) -> Dict[str, Any]:
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        # Metacognition runs in fabric.process — keep pipeline.metacog None to avoid double routing.
        pipeline = Pipeline(gate=gate, metacog=None)

        self.layers["gate"] = gate
        self.layers["pipeline"] = pipeline

        try:
            from cos.stream import SigmaStream

            self.layers["stream"] = SigmaStream(gate=gate)
        except ImportError:
            pass

        try:
            from cos.probe import SigmaProbe, SignalCascade

            self.layers["cascade"] = SignalCascade()
            self.layers["probe"] = SigmaProbe()
        except ImportError:
            pass

        try:
            from cos.metacog import SigmaMetacog

            self.layers["metacog"] = SigmaMetacog()
        except ImportError:
            pass

        try:
            from cos.reason import SigmaReason

            self.layers["reason"] = SigmaReason()
        except ImportError:
            pass

        try:
            from cos.calibrate import SigmaCalibrator

            self.layers["calibrator"] = SigmaCalibrator()
        except ImportError:
            pass

        try:
            from cos.depth import AdaptiveDepth, DepthRouter

            self.layers["depth"] = AdaptiveDepth(gate=gate)
            self.layers["depth_router"] = DepthRouter(gate=gate)
        except ImportError:
            pass

        try:
            from cos.moe import SigmaMoE

            moe = SigmaMoE(gate=gate)
            moe.add_expert("fast", cost=0.1, max_sigma=0.3)
            moe.add_expert("verify", cost=0.5, min_sigma=0.3, max_sigma=0.7)
            moe.add_expert("deep", cost=1.0, min_sigma=0.7)
            self.layers["moe"] = moe
        except ImportError:
            pass

        try:
            from cos.latent import SigmaLatent

            self.layers["latent"] = SigmaLatent(gate=gate)
        except ImportError:
            pass

        try:
            from cos.swarm import SigmaSwarm

            self.layers["swarm"] = SigmaSwarm(gate=gate)
        except ImportError:
            pass

        try:
            from cos.snapshot import ConversationHistory, SnapshotManager

            self.layers["history"] = ConversationHistory()
            snap_dir = self._snapshot_dir
            if snap_dir is None:
                snap_dir = Path(tempfile.mkdtemp(prefix="cos_fabric_snap_"))
            self.layers["snapshots"] = SnapshotManager(pipeline, snapshot_dir=snap_dir)
        except ImportError:
            pass

        self.booted = True
        return {"booted": True, "layers": list(self.layers.keys())}

    def process(
        self,
        prompt: str,
        response: Optional[str] = None,
        model: Any = None,
        **kwargs: Any,
    ) -> FabricResult:
        if not self.booted:
            self.boot()

        trace = SigmaTrace()
        pipeline: Pipeline = self.layers["pipeline"]

        depth_router = self.layers.get("depth_router")
        if depth_router:
            max_lp = int(kwargs.get("max_depth_loops", 8) or 8)
            allocated_loops = int(depth_router.allocate(str(prompt), max_loops=max_lp))
            trace.add(
                "depth_router",
                round(allocated_loops / max(max_lp, 1), 4),
                f"allocated_{allocated_loops}_loops",
            )

        run_kw: Dict[str, Any] = dict(kwargs)
        if model is not None:
            run_kw["model"] = model
        if response is not None:
            run_kw["response"] = response

        run_kw.pop("max_depth_loops", None)

        result = pipeline.run(str(prompt), **run_kw)
        trace.add("pipeline", result.sigma, result.verdict)
        self.sigma_bus.append(float(result.sigma))

        if result.verdict == "BLOCKED":
            trace.final_verdict = "BLOCKED"
            return self._wrap(result, trace)

        metacog = self.layers.get("metacog")
        if metacog and result.text:
            sig = run_kw.get("signals")
            if not isinstance(sig, dict):
                sig = {}
            assessment = metacog.assess(prompt, result.text, result.sigma, sig)
            tot = float(assessment.get("total_sigma", 0.0))
            trace.add("metacog", tot, assessment.get("action"))
            self.sigma_bus.append(float(assessment.get("total_sigma", 0.0)))

            act = str(assessment.get("action", "respond"))
            if act == "clarify":
                trace.final_verdict = "CLARIFY"
                result = PipelineResult(
                    text=result.text,
                    sigma=float(result.sigma),
                    verdict=result.verdict,
                    reason="ambiguous_input",
                    attempt=result.attempt,
                )
            elif act == "abstain":
                trace.final_verdict = "ABSTAIN"
                result = PipelineResult(
                    text="I don't have a reliable answer for this.",
                    sigma=max(float(result.sigma), float(assessment.get("total_sigma", 0.0))),
                    verdict="ABSTAIN",
                    reason="metacog_abstain",
                    attempt=result.attempt,
                )
            elif act == "retrieve":
                trace.needs_retrieval = True

        reason = self.layers.get("reason")
        if reason and result.text and kwargs.get("facts") is not None:
            facts = kwargs["facts"]
            uniq = kwargs.get("uniqueness", [])
            if not isinstance(uniq, list):
                uniq = []
            consistency = reason.check_consistency(facts, uniqueness=uniq)
            trace.add(
                "reason",
                float(consistency.get("sigma", 0.0)),
                "consistent" if consistency.get("consistent") else "conflict",
            )
            self.sigma_bus.append(float(consistency.get("sigma", 0.0)))
            if not consistency.get("consistent", True):
                new_sigma = max(float(result.sigma), float(consistency.get("sigma", 0.0)))
                if new_sigma > 0.8:
                    trace.final_verdict = "ABSTAIN"
                    result = PipelineResult(
                        text="I don't have a reliable answer for this.",
                        sigma=new_sigma,
                        verdict="ABSTAIN",
                        reason="reason_conflict",
                        attempt=result.attempt,
                    )
                else:
                    result = PipelineResult(
                        text=result.text,
                        sigma=new_sigma,
                        verdict=result.verdict,
                        reason=result.reason,
                        attempt=result.attempt,
                    )

        calibrator = self.layers.get("calibrator")
        if calibrator and getattr(calibrator, "fitted", False) and result.text is not None:
            raw_sigma = float(result.sigma)
            cal_sigma = float(calibrator.calibrate(raw_sigma))
            trace.add("calibrate", cal_sigma, f"raw={raw_sigma:.4f}→cal={cal_sigma:.4f}")
            self.sigma_bus.append(cal_sigma)
            result = PipelineResult(
                text=result.text,
                sigma=cal_sigma,
                verdict=str(pipeline.gate._verdict(cal_sigma))
                if hasattr(pipeline.gate, "_verdict")
                else result.verdict,
                reason=result.reason,
                attempt=result.attempt,
            )

        history = self.layers.get("history")
        if history and result.text:
            history.add("user", str(prompt))
            history.add("assistant", result.text, sigma=result.sigma, verdict=result.verdict)

        if not trace.final_verdict:
            trace.final_verdict = result.verdict

        return self._wrap(result, trace)

    def _wrap(self, result: PipelineResult, trace: SigmaTrace) -> FabricResult:
        return FabricResult(
            text=result.text,
            sigma=float(result.sigma),
            verdict=str(trace.final_verdict or result.verdict),
            trace=trace,
            needs_retrieval=trace.needs_retrieval,
            reason=result.reason,
        )

    def status(self) -> Dict[str, Any]:
        hist = self.layers.get("history")
        return {
            "booted": self.booted,
            "layers": {name: type(obj).__name__ for name, obj in self.layers.items()},
            "layer_count": len(self.layers),
            "history_turns": len(hist.turns) if hist is not None else 0,
        }

    def checkpoint(self, label: Optional[str] = None) -> Dict[str, Any]:
        mgr = self.layers.get("snapshots")
        if mgr:
            return mgr.checkpoint(label)
        return {"error": "no snapshot manager"}

    def rollback(self, target: Any = None) -> Dict[str, Any]:
        mgr = self.layers.get("snapshots")
        if mgr:
            return mgr.rollback(target)
        return {"error": "no snapshot manager"}


__all__ = ["FabricResult", "SigmaFabric", "SigmaTrace"]

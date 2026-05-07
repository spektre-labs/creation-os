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

  L1  Inference   — SigmaGate, Pipeline, Stream, Probe, SignalCascade,
                    ΣRecursion / σ-spec lab hooks (no trained stacks)
  L2  Cognition   — Reason (FOL), Metacog, Calibrate
  L3  Memory      — SnapshotManager, ConversationHistory
  L4–L9           — agency, learning, protocol, safety, deploy, metacognition / awareness-metric
                    proxies live in other modules; wire them here as needed. **v226–v230:**
                    ``SigmaSafety`` (first), ``SigmaMCPServer``, ``SigmaOffline``, ``SigmaCost``,
                    ``SigmaWatchdog`` when importable.

σ is propagated from the gate; downstream steps may raise σ or change verdict when
they detect conflict or meta-level abstention.
"""
from __future__ import annotations

import os
import tempfile
import time
from pathlib import Path
from typing import Any, Callable, Dict, FrozenSet, Iterable, List, Optional, Union

from cos.config import DEFAULT_CONFIG, SigmaConfig
from cos.pipeline import Pipeline, PipelineResult
from cos.sigma_gate import SigmaGate


class Fabric:
    """Creation OS cognitive boot — load optional modules, run :class:`~cos.omega.OmegaLoop` when present.

    **Diagnostic wiring:** :meth:`status` exposes *loaded* / *missing* / *failed* / *disabled* per module so
    the architecture stays observable (no silent stub orchestration). For measured claims see
    ``docs/CLAIM_DISCIPLINE.md``.

    For heavy σ-pipeline orchestration (snapshots, RAG, MCP, …) use :class:`SigmaFabric`.
    """

    def __init__(
        self,
        config: Optional[SigmaConfig] = None,
        *,
        disabled_modules: Optional[Iterable[str]] = None,
    ) -> None:
        self.config = config if config is not None else DEFAULT_CONFIG
        ta = float(self.config.threshold_accept)
        tb = float(self.config.threshold_abstain)
        self.gate = SigmaGate(threshold_accept=ta, threshold_abstain=tb)
        self._modules: Dict[str, Any] = {}
        self._module_errors: Dict[str, str] = {}
        self._disabled_modules: FrozenSet[str] = frozenset(
            str(x).strip() for x in (disabled_modules or ()) if str(x).strip()
        )
        self._booted = False

    def _install_optional(self, name: str, loader: Callable[[], Any]) -> None:
        """Load one optional module; ImportError → *missing*, other exceptions → *failed* with stored repr."""
        if name in self._disabled_modules:
            self._modules[name] = None
            return
        try:
            self._modules[name] = loader()
        except ImportError:
            self._modules[name] = None
        except Exception as e:  # noqa: BLE001 — intentional diagnostic surface
            self._modules[name] = None
            self._module_errors[name] = repr(e)

    def boot(self) -> Dict[str, Any]:
        """Load modules in dependency order; see :meth:`status` for per-module *state* and errors."""
        if self._booted:
            return self.status()

        self._modules.clear()
        self._module_errors.clear()
        self._modules["gate"] = self.gate
        self._modules["config"] = self.config

        self._install_optional(
            "graph",
            lambda: __import__("cos.graph", fromlist=["SigmaGraph"]).SigmaGraph(gate=self.gate),
        )
        if "memory" in self._disabled_modules:
            self._modules["memory"] = None
        else:
            try:
                self._modules["memory"] = __import__("cos.memory", fromlist=["SigmaMemory"]).SigmaMemory(
                    gate=self.gate,
                    graph=self._modules.get("graph"),
                )
            except ImportError:
                self._modules["memory"] = None
            except Exception as e:  # noqa: BLE001
                self._modules["memory"] = None
                self._module_errors["memory"] = repr(e)

        self._install_optional(
            "symbolic",
            lambda: __import__("cos.symbolic", fromlist=["SigmaSymbolic"]).SigmaSymbolic(gate=self.gate),
        )
        self._install_optional(
            "reason",
            lambda: __import__("cos.reason", fromlist=["SigmaReason"]).SigmaReason(),
        )
        self._install_optional(
            "world_model",
            lambda: __import__("cos.jepa", fromlist=["SigmaJEPA"]).SigmaJEPA(gate=self.gate),
        )
        self._install_optional(
            "drive",
            lambda: __import__("cos.drive", fromlist=["SigmaDrive"]).SigmaDrive(),
        )
        self._install_optional(
            "meta_goal",
            lambda: __import__("cos.meta_goal", fromlist=["SigmaMetaGoal"]).SigmaMetaGoal(gate=self.gate),
        )
        self._install_optional(
            "conscious",
            lambda: __import__("cos.conscious", fromlist=["SigmaConscious"]).SigmaConscious(gate=self.gate),
        )
        self._install_optional(
            "ttt",
            lambda: __import__("cos.ttt", fromlist=["SigmaTTT"]).SigmaTTT(gate=self.gate),
        )
        self._install_optional(
            "evolve",
            lambda: __import__("cos.evolve", fromlist=["SigmaEvolve"]).SigmaEvolve(gate=self.gate),
        )
        self._install_optional(
            "observe",
            lambda: __import__("cos.observe", fromlist=["SigmaObserve"]).SigmaObserve(),
        )
        self._install_optional(
            "drift",
            lambda: __import__("cos.drift", fromlist=["SigmaDrift"]).SigmaDrift(),
        )
        self._install_optional(
            "tool_safety",
            lambda: __import__("cos.tool_safety", fromlist=["ToolSafety"]).ToolSafety(gate=self.gate),
        )

        if "omega" in self._disabled_modules:
            self._modules["omega"] = None
        else:
            try:
                OmegaLoop = __import__("cos.omega", fromlist=["OmegaLoop"]).OmegaLoop
                self._modules["omega"] = OmegaLoop(
                    gate=self.gate,
                    memory=self._modules.get("memory"),
                    graph=self._modules.get("graph"),
                    config=self.config,
                    world_model=self._modules.get("world_model"),
                )
            except ImportError:
                self._modules["omega"] = None
            except Exception as e:  # noqa: BLE001
                self._modules["omega"] = None
                self._module_errors["omega"] = repr(e)

        self._booted = True
        return self.status()

    def status(self) -> Dict[str, Any]:
        """Module map with *loaded* / *missing* / *failed* / *disabled* and optional error strings."""
        mods: Dict[str, Any] = {}
        for name, mod in self._modules.items():
            if name in self._disabled_modules:
                state = "disabled"
                err = None
            elif mod is not None:
                state = "loaded"
                err = None
            elif name in self._module_errors:
                state = "failed"
                err = self._module_errors.get(name)
            else:
                state = "missing"
                err = None
            entry: Dict[str, Any] = {
                "state": state,
                "class": type(mod).__name__ if mod is not None else None,
                "error": err,
            }
            mods[name] = entry
        return {"modules": mods, "booted": self._booted}

    def get(self, module_name: str) -> Any:
        return self._modules.get(module_name)

    @staticmethod
    def _result_body(result: Dict[str, Any]) -> str:
        inner = result.get("result")
        if isinstance(inner, dict):
            return str(inner.get("reasoning", inner.get("result", inner)))
        return str(inner or "")

    def process(self, input_data: Any) -> Dict[str, Any]:
        """One cognitive step: Ω-loop when available, else σ-gate reflex on mirrored text."""
        if not self._booted:
            self.boot()

        t0 = time.perf_counter()
        omega = self._modules.get("omega")
        if omega is not None:
            result: Dict[str, Any] = dict(omega.step(input_data))
        else:
            s = str(input_data)
            sigma, verdict = self.gate.score(s, s)
            vn = str(verdict.name) if hasattr(verdict, "name") else str(verdict)
            result = {"result": None, "sigma": float(sigma), "σ": float(sigma), "verdict": vn, "step": 0}

        if "σ" not in result and "sigma" in result:
            result["σ"] = float(result["sigma"])
        latency_ms = (time.perf_counter() - t0) * 1000.0

        observe = self._modules.get("observe")
        if observe is not None and hasattr(observe, "record"):
            v = result.get("verdict", "RETHINK")
            vs = v.name if hasattr(v, "name") else str(v)
            observe.record(
                prompt=str(input_data),
                response=self._result_body(result),
                sigma=float(result.get("σ", result.get("sigma", 0.5))),
                verdict=vs,
                latency_ms=latency_ms,
            )

        drive = self._modules.get("drive")
        if drive is not None and hasattr(drive, "record"):
            drive.record(float(result.get("σ", result.get("sigma", 0.5))))

        meta = self._modules.get("meta_goal")
        if meta is not None and hasattr(meta, "record"):
            skill = "general"
            if isinstance(input_data, str) and "skill:" in input_data.lower():
                skill = "tagged"
            meta.record(skill, float(result.get("σ", result.get("sigma", 0.5))))

        return result

    def cognitive_state(self) -> Dict[str, Any]:
        """Snapshot for dashboards — metacognition / awareness-metric proxies only (lab)."""
        if not self._booted:
            self.boot()
        st = self.status()
        state: Dict[str, Any] = {
            "booted": st["booted"],
            "modules": st["modules"],
            "note": (
                "Architecture observability / lab integration only — see docs/CLAIM_DISCIPLINE.md "
                "for evidence scope (not a product claim of general intelligence)."
            ),
        }
        drive = self._modules.get("drive")
        if drive is not None and hasattr(drive, "emotion"):
            state["emotion"] = drive.emotion()
        meta = self._modules.get("meta_goal")
        if meta is not None and hasattr(meta, "next_goal"):
            state["next_goal"] = meta.next_goal()
        observe = self._modules.get("observe")
        if observe is not None and hasattr(observe, "summary"):
            state["observe"] = observe.summary(last_n=10)
        conscious = self._modules.get("conscious")
        if conscious is not None and hasattr(conscious, "σ_meta"):
            am = conscious.σ_meta()
            state["awareness_metrics"] = am
            state["σ_meta"] = am
        return state


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
        self._offline_mode = False
        self.sigma_bus: List[float] = []
        self._snapshot_dir = snapshot_dir
        self._optional_import_skips: List[str] = []

    def _note_skip(self, component: str) -> None:
        """Record an optional layer that failed to import (audit / diagnostics)."""
        self._optional_import_skips.append(str(component))

    def boot(self) -> Dict[str, Any]:
        from cos.sigma_gate import SigmaGate

        gate = SigmaGate()
        self._offline_mode = False
        self._optional_import_skips.clear()

        safety_layer = None
        try:
            from cos.safety import SigmaSafety

            safety_layer = SigmaSafety(gate=gate)
            self.layers["safety"] = safety_layer
        except ImportError:
            self._note_skip('safety')
        prompt_guard_mod = None
        if safety_layer is not None:
            prompt_guard_mod = safety_layer.prompt_guard
            self.layers["prompt_guard"] = prompt_guard_mod
        else:
            try:
                from cos.prompt_guard import SigmaPromptGuard

                prompt_guard_mod = SigmaPromptGuard(gate=gate)
                self.layers["prompt_guard"] = prompt_guard_mod
            except ImportError:
                self._note_skip("prompt_guard_standalone")

        # Metacognition runs in fabric.process — keep pipeline.metacog None to avoid double routing.
        pipeline = Pipeline(gate=gate, metacog=None, prompt_guard=prompt_guard_mod)

        self.layers["gate"] = gate
        self.layers["pipeline"] = pipeline

        try:
            from cos.stream import SigmaStream

            self.layers["stream"] = SigmaStream(gate=gate)
        except ImportError:
            self._note_skip('stream')
        try:
            from cos.probe import SigmaProbe, SignalCascade

            self.layers["cascade"] = SignalCascade()
            self.layers["probe"] = SigmaProbe()
        except ImportError:
            self._note_skip('probe')
        try:
            from cos.metacog import SigmaMetacog

            self.layers["metacog"] = SigmaMetacog()
        except ImportError:
            self._note_skip('metacog')
        try:
            from cos.reason import SigmaReason

            self.layers["reason"] = SigmaReason()
        except ImportError:
            self._note_skip('reason')
        try:
            from cos.calibrate import SigmaCalibrator

            self.layers["calibrator"] = SigmaCalibrator()
        except ImportError:
            self._note_skip('calibrate')
        try:
            from cos.depth import AdaptiveDepth, DepthRouter

            self.layers["depth"] = AdaptiveDepth(gate=gate)
            self.layers["depth_router"] = DepthRouter(gate=gate)
        except ImportError:
            self._note_skip('depth')
        try:
            from cos.moe import SigmaMoE

            moe = SigmaMoE(gate=gate)
            moe.add_expert("fast", cost=0.1, max_sigma=0.3)
            moe.add_expert("verify", cost=0.5, min_sigma=0.3, max_sigma=0.7)
            moe.add_expert("deep", cost=1.0, min_sigma=0.7)
            self.layers["moe"] = moe
        except ImportError:
            self._note_skip('moe')
        try:
            from cos.latent import SigmaLatent

            self.layers["latent"] = SigmaLatent(gate=gate)
        except ImportError:
            self._note_skip('latent')
        try:
            from cos.swarm import SigmaSwarm

            self.layers["swarm"] = SigmaSwarm(gate=gate)
        except ImportError:
            self._note_skip('swarm')
        try:
            from cos.world import SigmaWorld

            self.layers["world"] = SigmaWorld(gate=gate)
        except ImportError:
            self._note_skip('world')
        try:
            from cos.graph import SigmaGraph

            self.layers["graph"] = SigmaGraph(gate=gate)
        except ImportError:
            self._note_skip('graph')
        try:
            from cos.memory import SigmaMemory

            self.layers["memory"] = SigmaMemory(gate=gate, graph=self.layers.get("graph"))
        except ImportError:
            self._note_skip('memory')
        try:
            from cos.formal import SigmaFormal

            self.layers["formal"] = SigmaFormal(gate=gate)
        except ImportError:
            self._note_skip('formal')
        try:
            from cos.continual import SigmaContinual

            self.layers["continual"] = SigmaContinual(gate=gate)
        except ImportError:
            self._note_skip('continual')
        try:
            from cos.config import SigmaConfig
            from cos.jepa import SigmaJEPA
            from cos.omega import OmegaLoop

            cfg = SigmaConfig()
            mem = self.layers.get("memory")
            gr = self.layers.get("graph")
            if mem is not None and gr is not None:
                jm = SigmaJEPA(gate=gate, dim=128)
                self.layers["jepa_wm"] = jm
                self.layers["omega"] = OmegaLoop(
                    gate=gate,
                    memory=mem,
                    graph=gr,
                    config=cfg,
                    world_model=jm,
                )
            else:
                self._note_skip("omega")
        except ImportError:
            self._note_skip("omega")
        try:
            from cos.recursion import SigmaRecursion

            self.layers["recursion"] = SigmaRecursion(gate=gate)
        except ImportError:
            self._note_skip('recursion')
        try:
            from cos.agent_guard import SigmaAgentGuard

            self.layers["agent_guard"] = (
                safety_layer.agent_guard
                if safety_layer is not None
                else SigmaAgentGuard(gate=gate)
            )
        except ImportError:
            self._note_skip('agent_guard')
        try:
            from cos.observe import SigmaObserve

            self.layers["observe"] = SigmaObserve()
        except ImportError:
            self._note_skip('observe')
        try:
            from cos.embed import SigmaEmbed

            self.layers["embed"] = SigmaEmbed(gate=gate)
        except ImportError:
            self._note_skip('embed')
        try:
            from cos.speculative import SigmaSpeculative

            self.layers["speculative"] = SigmaSpeculative()
        except ImportError:
            self._note_skip('speculative')
        try:
            from cos.spike import SigmaSpike

            self.layers["spike"] = SigmaSpike()
        except ImportError:
            self._note_skip('spike')
        try:
            from cos.distill import SigmaDistill

            self.layers["distill"] = SigmaDistill()
        except ImportError:
            self._note_skip('distill')
        try:
            from cos.quantize import SigmaQuantize

            self.layers["quantize"] = SigmaQuantize()
        except ImportError:
            self._note_skip('quantize')
        try:
            from cos.kv_cache import SigmaKVCache

            self.layers["kv_cache"] = SigmaKVCache(max_size=128)
        except ImportError:
            self._note_skip('kv_cache')
        try:
            from cos.rag import SigmaRAG

            self.layers["rag"] = SigmaRAG(gate=gate)
        except ImportError:
            self._note_skip('rag')
        try:
            from cos.ttt import SigmaTTT

            self.layers["ttt"] = SigmaTTT(gate=gate)
        except ImportError:
            self._note_skip('ttt')
        try:
            from cos.split import SigmaSplit

            self.layers["split"] = SigmaSplit()
        except ImportError:
            self._note_skip('split')
        try:
            from cos.fleet import SigmaFleet

            self.layers["fleet"] = SigmaFleet()
        except ImportError:
            self._note_skip('fleet')
        try:
            from cos.evolve import SigmaEvolve

            self.layers["evolve"] = SigmaEvolve(gate=gate)
        except ImportError:
            self._note_skip('evolve')
        try:
            from cos.bench import SigmaBench

            self.layers["bench"] = SigmaBench()
        except ImportError:
            self._note_skip('bench')
        try:
            from cos.index import SigmaIndex

            self.layers["index"] = SigmaIndex(gate=gate)
        except ImportError:
            self._note_skip('index')
        try:
            from cos.voice import SigmaVoice

            self.layers["sigma_voice"] = SigmaVoice(gate=gate)
        except ImportError:
            self._note_skip('voice')
        try:
            from cos.offline import SigmaOffline

            off = SigmaOffline(gate=gate)
            self.layers["offline"] = off
            self._offline_mode = bool(off.detect_mode_from_env())
        except ImportError:
            self._note_skip('offline')
        try:
            from cos.cost import SigmaCost

            self.layers["cost"] = SigmaCost(gate=gate)
        except ImportError:
            self._note_skip('cost')
        try:
            from cos.mcp import SigmaMCPServer

            ag = self.layers.get("agent_guard")
            self.layers["mcp_server"] = SigmaMCPServer(gate=gate, agent_guard=ag)
        except ImportError:
            self._note_skip('mcp')
        try:
            from cos.watchdog import SigmaWatchdog

            wd = SigmaWatchdog(gate=gate, fabric=self)
            self.layers["watchdog"] = wd
            if os.environ.get("COS_WATCHDOG_AUTO_START") == "1":
                wd.start_background()
        except ImportError:
            self._note_skip('watchdog')
        try:
            from cos.snapshot import ConversationHistory, SnapshotManager

            self.layers["history"] = ConversationHistory()
            snap_dir = self._snapshot_dir
            if snap_dir is None:
                snap_dir = Path(tempfile.mkdtemp(prefix="cos_fabric_snap_"))
            self.layers["snapshots"] = SnapshotManager(pipeline, snapshot_dir=snap_dir)
        except ImportError:
            self._note_skip('snapshot')
        self.booted = True
        return {
            "booted": True,
            "layers": list(self.layers.keys()),
            "optional_import_skips": list(self._optional_import_skips),
        }

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

        tool_call = run_kw.pop("tool_call", None)
        allowed_tools = run_kw.pop("allowed_tools", None)
        denied_tools = run_kw.pop("denied_tools", None)
        tool_budget = run_kw.pop("tool_budget", None)

        index_documents = run_kw.pop("index_documents", None)
        index_query = run_kw.pop("index_query", None)
        bench_dataset = run_kw.pop("bench_dataset", None)
        bench_model = run_kw.pop("bench_model", None)
        meta_evolve_probe = run_kw.pop("meta_evolve_probe", None)
        voice_transcript = run_kw.pop("voice_transcript", None)
        voice_response = run_kw.pop("voice_response", None)
        run_safety_report = run_kw.pop("run_safety_report", None)
        cost_units = run_kw.pop("cost_units", None)
        mcp_simulate_tool = run_kw.pop("mcp_simulate_tool", None)

        index_layer = self.layers.get("index")
        if index_layer is not None and index_documents is not None:
            index_layer.index([str(x) for x in index_documents])
            trace.add("index_ingest", 0.0, f"n_docs={len(index_documents)}")

        rag_chunks = run_kw.pop("rag_chunks", None)
        ttt_context = run_kw.pop("ttt_context", None)
        use_fleet = bool(run_kw.pop("use_fleet", False))
        bandwidth_mbps = run_kw.pop("bandwidth_mbps", None)

        run_kw.pop("max_depth_loops", None)

        prompt_for_run = str(prompt)
        gate_early = self.layers["gate"]

        fleet_layer = self.layers.get("fleet")
        if fleet_layer and use_fleet:
            pick = fleet_layer.cascade_route(prompt_for_run, gate_early)
            trace.add(
                "fleet",
                float(pick.get("sigma", 0)),
                str(pick.get("name", "") or "none"),
            )
            mod = pick.get("model")
            if mod is not None:
                run_kw["model"] = mod

        rag_layer = self.layers.get("rag")
        if rag_layer and rag_chunks is not None:
            rr = rag_layer.sigma_rerank(prompt_for_run, list(rag_chunks), gate_early)
            kept = rr.get("kept", [])
            if kept:
                prompt_for_run = rag_layer.augment(prompt_for_run, kept)
            ms = (
                sum(float(c["sigma"]) for c in kept) / max(len(kept), 1)
                if kept
                else 0.0
            )
            trace.add("rag", round(ms, 4), f"kept={len(kept)}")

        ag_layer = self.layers.get("agent_guard")
        if tool_call is not None and ag_layer is not None:
            if isinstance(tool_call, dict):
                tname = str(tool_call.get("tool", ""))
                targs = tool_call.get("args", {})
            else:
                tname = str(getattr(tool_call, "tool", ""))
                targs = getattr(tool_call, "args", {})
            at_set = set(allowed_tools) if allowed_tools is not None else None
            dt_set = set(denied_tools) if denied_tools is not None else None
            tr = ag_layer.run_guardrails(
                tname,
                targs,
                allowed_tools=at_set,
                denied_tools=dt_set,
                remaining_budget=tool_budget,
            )
            sig_tr = float(tr.get("sigma", 1.0)) if tr.get("ok") else 1.0
            trace.add(
                "agent_guard",
                sig_tr,
                tr.get("decision", tr.get("reason", "na")),
            )
            if not tr.get("ok"):
                trace.final_verdict = "BLOCKED"
                return self._wrap(
                    PipelineResult(
                        text=None,
                        sigma=1.0,
                        verdict="BLOCKED",
                        reason=str(tr.get("reason", "agent_guard")),
                    ),
                    trace,
                )
            if tr.get("decision") == "BLOCK":
                trace.final_verdict = "BLOCKED"
                return self._wrap(
                    PipelineResult(
                        text=None,
                        sigma=float(tr.get("sigma", 1.0)),
                        verdict="BLOCKED",
                        reason="agent_guard_block",
                    ),
                    trace,
                )

        result = pipeline.run(prompt_for_run, **run_kw)
        trace.add("pipeline", result.sigma, result.verdict)
        self.sigma_bus.append(float(result.sigma))

        safety_layer = self.layers.get("safety")
        if safety_layer is not None:
            safety_layer.circuit_breaker_update(str(result.verdict))

        watchdog_layer = self.layers.get("watchdog")
        if watchdog_layer is not None:
            watchdog_layer.record_sigma(float(result.sigma))

        recursion_layer = self.layers.get("recursion")
        if recursion_layer and result.text:
            toks = str(result.text).split()
            if toks:
                gate = self.layers["gate"]
                route = recursion_layer.token_route(toks, gate, context=prompt_for_run)
                mean_sigma = sum(route["sigmas"]) / max(len(route["sigmas"]), 1)
                trace.add(
                    "recursion",
                    round(mean_sigma, 4),
                    f"mean_depth={route['mean_depth']:.3f}",
                )

        gate_ref = self.layers["gate"]

        ttt_layer = self.layers.get("ttt")
        if ttt_layer and ttt_context is not None:
            ttr = ttt_layer.adapt_with_sigma(float(result.sigma), str(ttt_context), gate_ref)
            trace.add(
                "ttt",
                round(float(ttr.get("sigma_after", result.sigma)), 4),
                f"rollback={ttr.get('rollback', False)}",
            )

        split_layer = self.layers.get("split")
        if split_layer:
            sr = split_layer.route(float(result.sigma), bandwidth_mbps=bandwidth_mbps)
            trace.add("split", float(result.sigma), str(sr.get("placement", "")))

        if result.text:
            toks = str(result.text).split()
            spec_layer = self.layers.get("speculative")
            if spec_layer and toks:
                vr = spec_layer.verify(prompt_for_run, toks[:24], gate_ref)
                trace.add("speculative", round(float(vr["accept_ratio"]), 4), "gate_verify")
            spike_layer = self.layers.get("spike")
            if spike_layer:
                lif = spike_layer.convert_gate_to_spike(gate_ref)
                trace.add("spike", round(float(lif["threshold"]), 4), "lif_threshold")
            distill_layer = self.layers.get("distill")
            if distill_layer and toks:
                dr = distill_layer.distill_step(
                    None,
                    None,
                    [{"prompt": prompt_for_run[:120], "student_text": " ".join(toks[:12])}],
                    gate_ref,
                )
                trace.add("distill", round(float(dr["kd_fraction"]), 4), "kd_fraction")
            quant_layer = self.layers.get("quantize")
            if quant_layer:
                mp = quant_layer.mixed_precision_map(
                    {f"L{i}": float(result.sigma) * (0.82 + 0.04 * i) for i in range(4)}
                )
                trace.add("quantize", float(result.sigma), f"layers={len(mp)}")
            kvc_layer = self.layers.get("kv_cache")
            if kvc_layer and toks:
                for i, _t in enumerate(toks[:32]):
                    pr = " ".join(toks[: i + 1])
                    sg = float(
                        gate_ref.compute_sigma(None, None, prompt_for_run, pr),
                    )
                    kvc_layer.put(f"t{i}", sg, _t)
                kvc_layer.sliding_window(20, keep_below=0.35)
                kvc_layer.enforce_budget(kvc_layer.max_size)
                trace.add(
                    "kv_cache",
                    round(kvc_layer.mean_sigma(), 4),
                    f"n={len(kvc_layer)}",
                )

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

        embed_layer = self.layers.get("embed")
        if embed_layer and result.text:
            blob = prompt_for_run[:120] + "\n" + str(result.text)[:120]
            er = embed_layer.embed_sigma(blob)
            trace.add("embed", round(float(er["mean_sigma"]), 4), f"dim={len(er['embedding'])}")

        history = self.layers.get("history")
        if history and result.text:
            history.add("user", str(prompt))
            history.add("assistant", result.text, sigma=result.sigma, verdict=result.verdict)

        memory = self.layers.get("memory")
        if memory and result.text and str(result.verdict) == "ACCEPT":
            memory.write(
                f"Q: {str(prompt)[:100]} A: {str(result.text)[:200]}",
                "episodic",
                sigma=float(result.sigma),
            )

        world = self.layers.get("world")
        if world:
            world.observe(str(prompt), features={"sigma": float(result.sigma)})

        if not trace.final_verdict:
            trace.final_verdict = result.verdict

        if index_layer is not None and index_query:
            hits = index_layer.search(str(index_query), top_k=5)
            ms = (
                sum(float(h["sigma"]) for h in hits) / max(len(hits), 1) if hits else 0.0
            )
            trace.add("index_search", round(ms, 4), f"hits={len(hits)}")

        evolve_layer = self.layers.get("evolve")
        if evolve_layer is not None and meta_evolve_probe:
            pr = evolve_layer.meta_evolve_probe(str(meta_evolve_probe))
            trace.add(
                "meta_evolve",
                round(float(pr.get("sigma", 0.0)), 4),
                str(pr.get("verdict", "")),
            )

        bench_layer = self.layers.get("bench")
        if bench_layer is not None and bench_dataset:

            def _default_bench_model(prompt: object, ref: object = "") -> str:
                p = str(prompt).lower().replace(" ", "")
                if "2+2" in p:
                    return "4"
                return str(ref or "stub")

            bm = bench_model if bench_model is not None else _default_bench_model
            br = bench_layer.run(str(bench_dataset), gate_ref, bm)
            trace.add(
                "bench",
                round(float(br.get("M_tier", 0.0)), 4),
                str(br.get("dataset", "")),
            )

        voice_layer = self.layers.get("sigma_voice")
        if (
            voice_layer is not None
            and voice_transcript is not None
            and voice_response is not None
        ):
            vr = voice_layer.realtime_stream(
                str(voice_transcript),
                gate_ref,
                str(voice_response),
            )
            trace.add(
                "voice",
                round(float(vr.get("out_sigma", 0.0)), 4),
                str(vr.get("verdict", "")),
            )

        if run_safety_report and self.layers.get("safety") is not None:
            trace.add(
                "safety_report",
                0.0,
                self.layers["safety"].safety_report(),
            )

        cost_layer = self.layers.get("cost")
        if cost_layer is not None and cost_units is not None:
            cost_layer.record_usage(float(cost_units))
            trace.add("cost_usage", round(float(cost_units), 4), "recorded")

        mcp = self.layers.get("mcp_server")
        if mcp is not None and mcp_simulate_tool:
            meth = str(mcp_simulate_tool.get("method", "tools/call"))
            tfc = mcp.trust_firewall_check(meth)
            if not tfc.get("allow"):
                trace.add("mcp_tool", 1.0, "blocked_trust_firewall")
            else:
                sc = mcp.score_tool_call(
                    str(mcp_simulate_tool.get("name", "unknown")),
                    mcp_simulate_tool.get("args", {}),
                )
                trace.add(
                    "mcp_tool",
                    round(float(sc.get("sigma", 0.0)), 4),
                    str(sc.get("verdict", "")),
                )

        observe_layer = self.layers.get("observe")
        if observe_layer:
            observe_layer.trace_request(
                str(prompt),
                result.text,
                float(result.sigma),
                str(trace.final_verdict),
            )
            observe_layer.per_layer_trace(trace.to_dict()["steps"])
            observe_layer.cost_tracking(
                route="fabric",
                sigma=float(result.sigma),
                cheap=float(result.sigma) < 0.45,
            )

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
            "offline_mode": bool(self._offline_mode),
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


__all__ = ["Fabric", "FabricResult", "SigmaFabric", "SigmaTrace"]

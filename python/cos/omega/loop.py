# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Ω-loop orchestrator: 14 σ-measured phases per turn (Python harness mirror of
``omega_phase_gates.{h,c}`` + legacy ``cos_omega_step`` stack).

This is a **lab scaffold** — swap real perception / JEPA / actuators in research code.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, List

from cos.sigma_gate_core import Verdict, sigma_gate, sigma_update

from .act import OmegaAct
from .continue_phase import OmegaContinue
from .consolidate import OmegaConsolidate
from .gate import OmegaGate
from .learn import OmegaLearn
from .perceive import OmegaPerceive
from .predict import OmegaPredict
from .prove import OmegaProve
from .reflect import OmegaReflect
from .remember import OmegaRemember
from .safety import OmegaSafety
from .simulate import OmegaSimulate
from .state import N_PHASES, OmegaContext, OmegaState, PHASE_NAMES
from .think import OmegaThink
from .watchdog import OmegaWatchdog


@dataclass
class OmegaLoop:
    """Ω = argmin ∫σ dt subject to ``K ≥ K_crit`` (discrete harness approximation)."""

    k_raw: float = 0.92
    perceive: Any = field(default_factory=OmegaPerceive)
    predict: Any = field(default_factory=OmegaPredict)
    remember: Any = field(default_factory=OmegaRemember)
    think: Any = field(default_factory=OmegaThink)
    gate: Any = field(default_factory=OmegaGate)
    safety: Any = field(default_factory=OmegaSafety)
    simulate: Any = field(default_factory=OmegaSimulate)
    act: Any = field(default_factory=OmegaAct)
    prove: Any = field(default_factory=OmegaProve)
    learn: Any = field(default_factory=OmegaLearn)
    reflect: Any = field(default_factory=OmegaReflect)
    consolidate: Any = field(default_factory=OmegaConsolidate)
    watchdog: Any = field(default_factory=OmegaWatchdog)
    should_continue: Any = field(default_factory=OmegaContinue)

    def __post_init__(self) -> None:
        if getattr(self.learn, "engram", None) is None and hasattr(self.remember, "engram"):
            self.learn.engram = self.remember.engram

    def _step_lane(self, ost: OmegaState, idx: int, sigma: float) -> Verdict:
        sigma_update(ost.phases[idx], max(0.0, min(1.0, sigma)), self.k_raw)
        return sigma_gate(ost.phases[idx])

    def _blend_master(self, ost: OmegaState, ctx: OmegaContext, sigma: float) -> None:
        b = ctx.scratch.get("_blend")
        if b is None:
            b = float(sigma)
        else:
            b = (float(b) + float(sigma)) / 2.0
        ctx.scratch["_blend"] = float(b)
        sigma_update(ost.master, max(0.0, min(1.0, b)), self.k_raw)

    def run(self, goal: str, *, max_turns: int = 50) -> List[Dict[str, Any]]:
        ost = OmegaState()
        history: List[Dict[str, Any]] = []
        lim = max(1, int(max_turns))

        for t in range(lim):
            ctx = OmegaContext(goal=str(goal or "").strip() or "Ω", turn=t)
            rec: Dict[str, Any] = {"turn": t, "phases": []}
            ctx.scratch["_blend"] = None

            raw, s0 = self.perceive(ctx, {"text": ctx.goal})
            v0 = self._step_lane(ost, 0, s0)
            self._blend_master(ost, ctx, s0)
            rec["phases"].append({"name": PHASE_NAMES[0], "sigma": s0, "verdict": int(v0)})

            pred, s1 = self.predict(ctx, raw)
            v1 = self._step_lane(ost, 1, s1)
            self._blend_master(ost, ctx, s1)
            rec["phases"].append({"name": PHASE_NAMES[1], "sigma": s1, "verdict": int(v1)})

            mem, s2 = self.remember(ctx, ctx.goal)
            v2 = self._step_lane(ost, 2, s2)
            self._blend_master(ost, ctx, s2)
            rec["phases"].append({"name": PHASE_NAMES[2], "sigma": s2, "verdict": int(v2)})

            thoughts, s3 = self.think(ctx, mem, pred)
            v3 = self._step_lane(ost, 3, s3)
            self._blend_master(ost, ctx, s3)
            ctx.scratch["thoughts"] = thoughts
            rec["phases"].append({"name": PHASE_NAMES[3], "sigma": s3, "verdict": int(v3)})

            blob = "\n".join(thoughts) if thoughts else ctx.goal
            scores = {
                "entropy": float(s0),
                "hide": float(s1),
                "icr": float(s2),
                "lsd": float(s3),
                "spectral": 0.35,
                "sae": None,
            }
            g_out, s4 = self.gate(ctx, scores)
            v4 = self._step_lane(ost, 4, s4)
            self._blend_master(ost, ctx, s4)
            rec["phases"].append({"name": PHASE_NAMES[4], "sigma": s4, "verdict": int(v4)})
            gv = Verdict(int(g_out["verdict"]))
            ctx.scratch["gate_verdict"] = gv

            sv, _rule = self.safety(ctx, blob)
            s5 = 0.88 if sv == Verdict.ABSTAIN else 0.2
            v5 = self._step_lane(ost, 5, s5)
            self._blend_master(ost, ctx, s5)
            rec["phases"].append({"name": PHASE_NAMES[5], "sigma": s5, "verdict": int(v5)})

            _sim_v, _out, s6 = self.simulate(ctx, blob)
            v6 = self._step_lane(ost, 6, s6)
            self._blend_master(ost, ctx, s6)
            rec["phases"].append({"name": PHASE_NAMES[6], "sigma": s6, "verdict": int(v6)})

            act_res, s7 = self.act(ctx, blob, ost.master)
            v7 = self._step_lane(ost, 7, s7)
            self._blend_master(ost, ctx, s7)
            rec["phases"].append({"name": PHASE_NAMES[7], "sigma": s7, "verdict": int(v7)})

            receipt = self.prove(ctx, blob, act_res, ost.master)
            s8 = 0.18
            v8 = self._step_lane(ost, 8, s8)
            self._blend_master(ost, ctx, s8)
            rec["phases"].append({"name": PHASE_NAMES[8], "sigma": s8, "verdict": int(v8)})
            rec["receipt"] = receipt

            self.learn(
                ctx,
                {"prompt": ctx.goal, "response": blob, "sigma": float(s4)},
                gv,
            )
            s9 = 0.22
            v9 = self._step_lane(ost, 9, s9)
            self._blend_master(ost, ctx, s9)
            rec["phases"].append({"name": PHASE_NAMES[9], "sigma": s9, "verdict": int(v9)})

            rec["max_sigma"] = max(float(p["sigma"]) for p in rec["phases"])
            status, s10 = self.reflect(history + [rec])
            v10 = self._step_lane(ost, 10, s10)
            self._blend_master(ost, ctx, s10)
            rec["phases"].append({"name": PHASE_NAMES[10], "sigma": s10, "verdict": int(v10)})
            rec["reflect"] = {"status": status, "sigma": s10}

            n_c = self.consolidate(getattr(self.remember, "engram", None))
            s11 = 0.25 if n_c == 0 else 0.15
            v11 = self._step_lane(ost, 11, s11)
            self._blend_master(ost, ctx, s11)
            rec["phases"].append({"name": PHASE_NAMES[11], "sigma": s11, "verdict": int(v11)})

            wd = self.watchdog(ost.master, t)
            v12 = self._step_lane(ost, 12, 0.9 if wd == Verdict.ABSTAIN else 0.12)
            self._blend_master(ost, ctx, 0.9 if wd == Verdict.ABSTAIN else 0.12)
            rec["phases"].append({"name": PHASE_NAMES[12], "sigma": 0.9 if wd == Verdict.ABSTAIN else 0.12, "verdict": int(v12)})
            rec["watchdog"] = int(wd)

            cont = self.should_continue(ost.master)
            s13 = 0.1 if cont else 0.88
            v13 = self._step_lane(ost, 13, s13)
            self._blend_master(ost, ctx, s13)
            rec["phases"].append({"name": PHASE_NAMES[13], "sigma": s13, "verdict": int(v13)})
            rec["continue"] = bool(cont)

            ost.turn = t + 1
            ost.history.append(rec)
            history.append(rec)

            if wd == Verdict.ABSTAIN:
                break
            if not cont:
                break

        return history


__all__ = ["OmegaLoop", "N_PHASES", "PHASE_NAMES"]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-JEPA lab scaffold: predict in a **latent** vector (no pixels), score prediction drift
with a σ-style scalar, and run the canonical **Q16** cognitive interrupt
(``sigma_gate_core`` — not ``sigma_gate.h``).

Architecture alignment (LeCun-style JEPA / world-model lab): **context encoder** observes a
visible crop, **predictor** proposes latent values for masked positions, **target encoder**
(EMA of context in trainable lab setups) encodes the held-out region. **σ** here is the
normalized latent mismatch (see ``compute_latent_sigma``), then fed through the same gate core
as other σ-scored paths.
"""
from __future__ import annotations

import math
import random
from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple, Union

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

K_RAW_DEFAULT = 0.92

# Observations tagged with this substring are treated as out-of-distribution for ``world_model_predict`` (lab).
LAB_UNKNOWN_OBS_TAG = "__COS_UNK__"


def _clamp01(x: float) -> float:
    if x < 0.0:
        return 0.0
    if x > 1.0:
        return 1.0
    return float(x)


def sigreg_collapse_penalty(latents: Sequence[Sequence[float]]) -> float:
    """
    **Lab** SIGReg-shaped penalty: low across-batch variance ⇒ collapse risk in ``[0,1]``.

    Not the Cramér–Wold SIGReg objective from cited papers — a cheap stand-in for training hooks.
    """
    rows = [list(map(float, z)) for z in latents if z is not None]
    if len(rows) < 2:
        return 0.0
    d = min(len(rows[0]), max((len(r) for r in rows), default=0))
    if d <= 0:
        return 0.0
    vars_: List[float] = []
    for j in range(d):
        col = [r[j] for r in rows if j < len(r)]
        if len(col) < 2:
            continue
        m = sum(col) / float(len(col))
        v = sum((c - m) ** 2 for c in col) / float(len(col))
        vars_.append(float(v))
    if not vars_:
        return 0.0
    mean_var = sum(vars_) / float(len(vars_))
    return _clamp01(1.0 - 12.0 * mean_var)


MaskSpec = Sequence[bool]


def apply_mask_vector(values: Sequence[float], mask: MaskSpec, *, keep_visible: bool) -> List[float]:
    """
    ``mask[i] == True`` marks a **masked** (hidden) index.

    * ``keep_visible=True`` keeps non-masked positions and zeros masked positions.
    * ``keep_visible=False`` keeps masked positions and zeros visible positions.
    """
    out: List[float] = []
    for i, v in enumerate(values):
        is_masked = bool(mask[i % len(mask)]) if mask else False
        fv = float(v)
        if keep_visible:
            out.append(0.0 if is_masked else fv)
        else:
            out.append(fv if is_masked else 0.0)
    return out


def generate_mask_uniform(
    length: int, mask_ratio: float = 0.5, *, rng: Optional[random.Random] = None
) -> List[bool]:
    r = rng or random.Random()
    n = max(1, int(length))
    p = float(mask_ratio)
    if p < 0.0:
        p = 0.0
    if p > 1.0:
        p = 1.0
    return [r.random() < p for _ in range(n)]


def compute_latent_sigma(predicted: Sequence[float], target: Sequence[float]) -> float:
    """Normalized L2 latent distance, ``sqrt(||p - t||^2 / (||t||^2 + eps))`` (not clamped)."""
    diff = 0.0
    norm = 0.0
    for p, t in zip(predicted, target):
        d = float(p) - float(t)
        diff += d * d
        tv = float(t)
        norm += tv * tv
    return math.sqrt(diff / (norm + 1e-8))


def compute_latent_sigma_masked(
    predicted: Sequence[float], target: Sequence[float], mask: Sequence[bool]
) -> float:
    """
    Same normalized L2 as ``compute_latent_sigma``, restricted to indices where ``mask[i]`` is True.

    This keeps the lab comparable when the target path only sees the held-out crop while the
    predictor copies visible slots from the context encoder unchanged.
    """
    mp: List[float] = []
    mt: List[float] = []
    n = min(len(predicted), len(target), len(mask))
    for i in range(n):
        if mask[i]:
            mp.append(float(predicted[i]))
            mt.append(float(target[i]))
    if not mp:
        return 0.0
    return compute_latent_sigma(mp, mt)


class JEPALabGate:
    """
    Thin σ-gate adapter on ``sigma_gate_core`` plus a simple **lab** uncertainty probe on latents.

    Swap in a custom ``gate`` object on ``SigmaJEPA`` with the same methods if needed.
    """

    def __init__(self, *, k_raw: float = K_RAW_DEFAULT) -> None:
        self.k_raw = float(k_raw)

    def score_from_sigma(self, sigma: float) -> Tuple[float, Verdict]:
        st = SigmaState()
        s = _clamp01(float(sigma))
        sigma_update(st, s, self.k_raw)
        verdict = sigma_gate(st)
        return s, verdict

    def estimate_prediction_uncertainty(self, predicted: Sequence[float]) -> float:
        if not predicted:
            return 1.0
        acc = sum(float(x) * float(x) for x in predicted)
        mag = math.sqrt(acc / (float(len(predicted)) + 1e-8))
        return _clamp01(mag * 1.15)


def _cosine_similarity_vecs(a: Sequence[float], b: Sequence[float]) -> float:
    if len(a) == 0 or len(b) == 0:
        return 0.0
    n = min(len(a), len(b))
    dot = 0.0
    na = 0.0
    nb = 0.0
    for i in range(n):
        x = float(a[i])
        y = float(b[i])
        dot += x * y
        na += x * x
        nb += y * y
    denom = math.sqrt(na) * math.sqrt(nb) + 1e-12
    return _clamp01(dot / denom)


class LatentEncoder(Protocol):
    def __call__(self, observation: Any) -> List[float]:
        ...

    def ema_copy(self) -> LatentEncoder:
        ...


class LatentPredictor(Protocol):
    def __call__(
        self,
        z: Sequence[float],
        action: Optional[str] = None,
        mask: Optional[MaskSpec] = None,
    ) -> List[float]:
        ...

    def uncertainty(self, z_current: Sequence[float], z_next: Sequence[float]) -> float:
        ...


class LabLatentEncoder:
    """Deterministic UTF-8 → bounded ``dim``-vector encoder (lab only)."""

    def __init__(self, dim: int = 8, *, ema_blend: float = 1.0) -> None:
        self.dim = int(dim)
        self._ema_blend = float(ema_blend)

    def ema_copy(self) -> LabLatentEncoder:
        return LabLatentEncoder(self.dim, ema_blend=0.97 * self._ema_blend + 0.03)

    def __call__(self, observation: Any) -> List[float]:
        if isinstance(observation, (list, tuple)):
            raw = [float(x) for x in observation[: self.dim]]
        else:
            s = str(observation).encode("utf-8", errors="ignore")[: self.dim]
            raw = [float(b) / 255.0 for b in s]
        raw = raw + [0.0] * (self.dim - len(raw))
        return [_clamp01(v * self._ema_blend) for v in raw]


class LabLatentPredictor:
    """Small latent drift + optional action bump (lab only)."""

    def __init__(self, drift: float = 0.02) -> None:
        self.drift = float(drift)

    def __call__(
        self,
        z: Sequence[float],
        action: Optional[str] = None,
        mask: Optional[MaskSpec] = None,
    ) -> List[float]:
        if mask is None:
            out = [_clamp01(float(z[i]) + self.drift + 0.005 * float(i % 5)) for i in range(len(z))]
            if action:
                bump = min(0.08, 0.004 * float(len(str(action))))
                out[0] = _clamp01(out[0] + bump)
            return out
        out = []
        for i in range(len(z)):
            is_masked = bool(mask[i % len(mask)])
            if not is_masked:
                out.append(float(z[i]))
            else:
                out.append(_clamp01(float(z[i]) + self.drift + 0.005 * float(i % 5)))
        if action:
            bump = min(0.08, 0.004 * float(len(str(action))))
            for i in range(len(out)):
                is_masked = bool(mask[i % len(mask)]) if mask else False
                if is_masked:
                    out[i] = _clamp01(out[i] + bump * 0.55)
                    break
        return out

    def uncertainty(self, z_current: Sequence[float], z_next: Sequence[float]) -> float:
        return 1.0 - _cosine_similarity_vecs(z_current, z_next)


class TrainableLatentEncoder:
    """Per-dimension lab encoderweights (for ``train_step`` + EMA target updates)."""

    def __init__(self, dim: int = 8) -> None:
        self.dim = int(dim)
        self._w = [1.0] * self.dim

    def ema_copy(self) -> TrainableLatentEncoder:
        o = TrainableLatentEncoder(self.dim)
        o._w = list(self._w)
        return o

    def __call__(self, observation: Any) -> List[float]:
        if isinstance(observation, (list, tuple)):
            raw = [float(x) for x in observation[: self.dim]]
        else:
            s = str(observation).encode("utf-8", errors="ignore")[: self.dim]
            raw = [float(b) / 255.0 for b in s]
        raw = raw + [0.0] * (self.dim - len(raw))
        return [_clamp01(raw[i] * self._w[i]) for i in range(self.dim)]

    def parameters(self) -> List[List[float]]:
        return [self._w]


class TrainableLatentPredictor(LabLatentPredictor):
    """Adds learnable residuals on masked JEPA slots for toy training loops."""

    def __init__(self, dim: int = 8, drift: float = 0.02) -> None:
        super().__init__(drift=drift)
        self.dim = int(dim)
        self._adj = [0.0] * self.dim

    def __call__(
        self,
        z: Sequence[float],
        action: Optional[str] = None,
        mask: Optional[MaskSpec] = None,
    ) -> List[float]:
        base = super().__call__(z, action, mask)
        return [_clamp01(base[i] + (self._adj[i] if i < len(self._adj) else 0.0)) for i in range(len(base))]

    def jepa_train_update(self, predicted: Sequence[float], target: Sequence[float], mask: MaskSpec) -> None:
        lr = 0.35
        for i in range(min(len(predicted), len(target), len(self._adj))):
            if i < len(mask) and mask[i]:
                self._adj[i] += lr * (float(target[i]) - float(predicted[i]))


class SigmaJEPA:
    """Encode → predict → measure (when a target latent is available)."""

    def __init__(
        self,
        encoder: LatentEncoder,
        predictor: LatentPredictor,
        gate: Optional[Any] = None,
        *,
        k_raw: float = K_RAW_DEFAULT,
        ema_decay: float = 0.996,
    ) -> None:
        self.context_encoder = encoder
        self.target_encoder = encoder.ema_copy()
        self.predictor = predictor
        self.gate = gate
        self.k_raw = float(k_raw)
        self.ema_decay = float(ema_decay)

    def _latent_dim(self) -> int:
        enc = self.context_encoder
        d = getattr(enc, "dim", None)
        if isinstance(d, int) and d > 0:
            return int(d)
        return len(list(enc(" ")))

    def _vectorize_observation(self, x: Union[Sequence[float], Any]) -> List[float]:
        if isinstance(x, (list, tuple)) and all(isinstance(v, (int, float)) for v in x):
            d = self._latent_dim()
            xs = [float(v) for v in x][:d]
            return xs + [0.0] * (d - len(xs))
        return list(self.context_encoder(x))

    def _normalize_mask(self, mask: MaskSpec, length: int) -> List[bool]:
        mlist = list(mask)
        if not mlist:
            return [True] * length
        if len(mlist) != length:
            return [(mlist[i % len(mlist)] if mlist else True) for i in range(length)]
        return [bool(b) for b in mlist]

    def _effective_gate(self) -> Any:
        if self.gate is not None and hasattr(self.gate, "estimate_prediction_uncertainty"):
            return self.gate
        return JEPALabGate(k_raw=self.k_raw)

    def score_from_sigma(self, sigma: float) -> Tuple[float, Verdict]:
        g = self.gate if self.gate is not None and hasattr(self.gate, "score_from_sigma") else JEPALabGate(
            k_raw=self.k_raw
        )
        return g.score_from_sigma(sigma)

    @staticmethod
    def cosine_similarity(a: Sequence[float], b: Sequence[float]) -> float:
        return _cosine_similarity_vecs(a, b)

    def encode(self, observation: Any) -> List[float]:
        return list(self.context_encoder(observation))

    def predict(
        self,
        z_current: Sequence[float],
        action: Optional[str] = None,
        mask: Optional[MaskSpec] = None,
    ) -> List[float]:
        return list(self.predictor(z_current, action, mask))

    def forward(self, x: Any, mask: MaskSpec) -> Dict[str, Any]:
        vec = self._vectorize_observation(x)
        m = self._normalize_mask(mask, len(vec))
        if not any(m):
            m = [i == 0 for i in range(len(vec))]

        vis_v = apply_mask_vector(vec, m, keep_visible=True)
        hid_v = apply_mask_vector(vec, m, keep_visible=False)

        context_repr = list(self.context_encoder(vis_v))
        predicted_repr = self.predict(context_repr, None, m)
        target_repr = list(self.target_encoder(hid_v))

        sigma_raw = float(compute_latent_sigma_masked(predicted_repr, target_repr, m))
        sigma = _clamp01(sigma_raw)
        _, verdict = self.score_from_sigma(sigma)
        if self.gate is not None and hasattr(self.gate, "after_verdict"):
            getattr(self.gate, "after_verdict")(sigma, verdict)

        return {
            "predicted": predicted_repr,
            "target": target_repr,
            "sigma": sigma,
            "sigma_raw": sigma_raw,
            "verdict": verdict,
            "understanding": float(1.0 - sigma),
            "mask": m,
        }

    def generate_mask(self, x: Any, *, rng: Optional[random.Random] = None) -> List[bool]:
        vec = self._vectorize_observation(x)
        m = generate_mask_uniform(len(vec), 0.45, rng=rng or random.Random())
        return self._normalize_mask(m, len(vec))

    def train_step(self, batch: Sequence[Any], *, rng: Optional[random.Random] = None) -> Dict[str, float]:
        r = rng or random.Random(0x5EED)
        total = 0.0
        n = 0
        for item in batch:
            mask = self.generate_mask(item, rng=r)
            result = self.forward(item, mask)
            total += float(result["sigma"])
            n += 1
            if hasattr(self.predictor, "jepa_train_update"):
                getattr(self.predictor, "jepa_train_update")(
                    result["predicted"], result["target"], result["mask"]
                )
            self.update_target_ema()
        return {"avg_sigma": total / max(1, n)}

    def update_target_ema(self) -> None:
        ctx = self.context_encoder
        tgt = self.target_encoder
        if not (hasattr(ctx, "parameters") and hasattr(tgt, "parameters")):
            return
        decay = float(self.ema_decay)
        for t_params, c_params in zip(tgt.parameters(), ctx.parameters()):
            for i in range(len(t_params)):
                t_params[i] = decay * float(t_params[i]) + (1.0 - decay) * float(c_params[i])

    @staticmethod
    def combine_obs_action(
        z_obs: Sequence[float], action: Optional[str], *, dim_hint: int
    ) -> List[float]:
        """Lab fusion: append normalized action mass into trailing slots."""
        z = [float(v) for v in z_obs]
        d = max(1, int(dim_hint))
        if len(z) < d:
            z = z + [0.0] * (d - len(z))
        z = z[:d]
        if action:
            bump = min(0.35, 0.03 * float(len(str(action))))
            z[-1] = _clamp01(z[-1] + bump)
        return z

    def world_model_predict(self, observation: Any, action: Optional[str] = None) -> Dict[str, Any]:
        obs_repr = self.encode(observation)
        if action is not None:
            combined = self.combine_obs_action(obs_repr, str(action), dim_hint=len(obs_repr))
            predicted_next = self.predict(combined, None, None)
        else:
            predicted_next = self.predict(obs_repr, None, None)

        gate = self._effective_gate()
        sigma = float(gate.estimate_prediction_uncertainty(predicted_next))
        if LAB_UNKNOWN_OBS_TAG in str(observation):
            sigma = _clamp01(sigma + 0.46)
        return {
            "predicted_state": predicted_next,
            "sigma": sigma,
            "confident": bool(sigma < 0.2),
        }

    def planning_step(self, current_state: Any, candidate_actions: Sequence[str]) -> Dict[str, Any]:
        best_action: Optional[str] = None
        best_sigma = 1.0
        for act in candidate_actions:
            a = str(act).strip()
            if not a:
                continue
            prediction = self.world_model_predict(current_state, a)
            sig = float(prediction["sigma"])
            if sig < best_sigma:
                best_sigma = sig
                best_action = act
        return {
            "action": best_action,
            "predicted_sigma": float(best_sigma),
            "confident": bool(best_sigma < 0.2),
        }

    def world_understanding(
        self,
        probes: Optional[Sequence[Any]] = None,
        *,
        rng: Optional[random.Random] = None,
    ) -> float:
        r = rng or random.Random(0xC0FFEE)
        items: List[Any] = list(probes) if probes is not None else [f"probe-{i}" for i in range(8)]
        if not items:
            return 1.0
        acc = 0.0
        for i, x in enumerate(items):
            mask = self.generate_mask(x, rng=random.Random(r.randint(0, 2**30) + i))
            acc += float(self.forward(x, mask)["understanding"])
        return acc / float(len(items))

    def measure_prediction_sigma(
        self, z_pred: Sequence[float], z_actual: Sequence[float]
    ) -> Tuple[float, Verdict]:
        similarity = _cosine_similarity_vecs(z_pred, z_actual)
        sigma = _clamp01(1.0 - similarity)
        st = SigmaState()
        sigma_update(st, sigma, self.k_raw)
        verdict = sigma_gate(st)
        if self.gate is not None and hasattr(self.gate, "after_verdict"):
            getattr(self.gate, "after_verdict")(sigma, verdict)
        return sigma, verdict

    def world_model_step(self, observation: Any, action: Optional[str] = None) -> Dict[str, Any]:
        z_current = self.encode(observation)
        z_pred = self.predict(z_current, action)
        return {
            "z_current": z_current,
            "z_predicted": z_pred,
            "ready_to_measure": True,
        }

    def update_on_observation(
        self, prediction: Dict[str, Any], next_observation: Any
    ) -> Dict[str, Any]:
        z_actual = list(self.target_encoder(next_observation))
        z_pred = prediction["z_predicted"]
        sigma, verdict = self.measure_prediction_sigma(z_pred, z_actual)
        return {
            "sigma_model": sigma,
            "verdict": verdict,
            "prediction_error": sigma,
            "world_model_quality": 1.0 - sigma,
        }

    def plan_argmin_sigma(
        self,
        observation: Any,
        candidate_plans: Sequence[Sequence[str]],
        *,
        horizon: int = 5,
        planning_steps: int = 100,
    ) -> Dict[str, Any]:
        """
        Evaluate up to ``planning_steps`` candidate action sequences; pick lowest final roll-out σ.
        """
        from cos.sigma_imagination import SigmaImagination

        img = SigmaImagination(self, k_raw=self.k_raw)
        budget = max(1, int(planning_steps))
        plans = list(candidate_plans)[:budget]
        best, sig = img.plan(observation, plans, horizon=int(horizon))
        batch_latents: List[List[float]] = [list(self.encode(observation))]
        if best:
            z = list(self.encode(observation))
            for a in list(best)[: int(horizon)]:
                z = list(self.predict(z, a))
                batch_latents.append(z)
        return {
            "best_plan": list(best) if best is not None else [],
            "sigma": float(sig),
            "evaluated": int(len(plans)),
            "sigreg_collapse_penalty": float(sigreg_collapse_penalty(batch_latents)),
        }

    def latent_trajectory_for_visualize(self, observation: Any, actions: Sequence[str], *, max_steps: int = 10) -> Dict[str, Any]:
        """JSON-safe polyline for ``cos think --visualize`` (lab)."""
        from cos.sigma_imagination import SigmaImagination

        img = SigmaImagination(self, k_raw=self.k_raw)
        acts = list(actions)[: int(max_steps)]
        roll = img.imagine(observation, acts, max_steps=int(max_steps))
        pts: List[List[float]] = []
        z = list(self.encode(observation))
        pts.append([round(float(v), 4) for v in z])
        for act in acts:
            z = list(self.predict(z, act))
            pts.append([round(float(v), 4) for v in z])
        collapse = float(sigreg_collapse_penalty(pts))
        return {"trajectory": roll["trajectory"], "latent_polyline": pts, "sigreg_collapse_penalty": collapse}


__all__ = [
    "JEPALabGate",
    "K_RAW_DEFAULT",
    "LAB_UNKNOWN_OBS_TAG",
    "LabLatentEncoder",
    "LabLatentPredictor",
    "LatentEncoder",
    "LatentPredictor",
    "SigmaJEPA",
    "TrainableLatentEncoder",
    "TrainableLatentPredictor",
    "apply_mask_vector",
    "compute_latent_sigma",
    "compute_latent_sigma_masked",
    "generate_mask_uniform",
    "sigreg_collapse_penalty",
]

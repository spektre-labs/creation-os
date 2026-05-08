# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-federated lab: toy FedAvg-style rounds with σ-thresholded acceptance and optional σ-weighting.

**Evidence class:** lab demo — vector-valued “models” are plain ``dict[str, list[float]]``, not
PyTorch. This does **not** prove robustness against adaptive attackers; it encodes policy
hooks aligned with ``sigma_gate_core``.

σ-adaptive client DP (noise + clip) lives in :mod:`cos.privacy` (:class:`cos.privacy.SigmaPrivacy`).

``python/cos/sigma_gate.h`` is **not** modified here.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
import math
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional, Tuple

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_q16, sigma_update_q16

# Flat model: each key maps to a 1-D list of floats (toy tensor).
WeightDict = Dict[str, List[float]]


def _deep_copy_weights(w: WeightDict) -> WeightDict:
    return {k: list(v) for k, v in w.items()}


def _mean_abs_delta(candidate: WeightDict, global_w: WeightDict) -> float:
    num = 0.0
    den = 0
    for k in global_w:
        for a, b in zip(candidate[k], global_w[k]):
            num += abs(float(a) - float(b))
            den += 1
    return num / float(max(den, 1))


def _l2_norm(w: WeightDict) -> float:
    s = 0.0
    for vals in w.values():
        for x in vals:
            s += float(x) * float(x)
    return math.sqrt(s)


@dataclass
class SigmaDifferentialPrivacy:
    """Lab Gaussian noise + L2 clipping (not a formal DP accountant)."""

    epsilon: float = 1.0
    delta: float = 1e-5

    def clip_update(self, update: WeightDict, max_norm: float = 1.0) -> WeightDict:
        n = _l2_norm(update)
        if n <= max_norm or n <= 0.0:
            return _deep_copy_weights(update)
        sc = max_norm / n
        return {k: [float(x) * sc for x in vals] for k, vals in update.items()}

    def add_noise(self, update: WeightDict, *, sensitivity: float = 1.0) -> WeightDict:
        scale = float(sensitivity) / max(float(self.epsilon), 1e-9)
        out: WeightDict = {}
        for k, vals in update.items():
            out[k] = [float(x) + random.gauss(0.0, scale) for x in vals]
        return out


class SigmaFederatedServer:
    """Server-side σ screening + inverse-σ FedAvg weights."""

    def __init__(
        self,
        global_weights: WeightDict,
        *,
        sigma_reject_threshold: float = 0.5,
        weight_eps: float = 0.01,
        dp: Optional[SigmaDifferentialPrivacy] = None,
        apply_dp_after_agg: bool = True,
    ) -> None:
        self.global_weights = _deep_copy_weights(global_weights)
        self.sigma_reject_threshold = float(sigma_reject_threshold)
        self.weight_eps = float(weight_eps)
        self.dp = dp
        self.apply_dp_after_agg = bool(apply_dp_after_agg)
        self.round = 0
        self.client_stats: Dict[str, Dict[str, Any]] = {}
        self.last_aggregate: Dict[str, Any] = {}

    def distribute(self, client_ids: List[str]) -> Dict[str, WeightDict]:
        return {cid: _deep_copy_weights(self.global_weights) for cid in client_ids}

    def score_update(self, update: WeightDict) -> float:
        """Heuristic suspicion score in [0, 1]: large moves relative to global → high."""
        mad = _mean_abs_delta(update, self.global_weights)
        return float(min(1.0, mad / 2.0))

    def _byzantine_trim(
        self, scored: List[Tuple[str, WeightDict, float]]
    ) -> List[Tuple[str, WeightDict, float]]:
        if len(scored) < 3:
            return scored
        sigs = sorted(s for _, _, s in scored)
        med = sigs[len(sigs) // 2]
        return [(cid, u, s) for cid, u, s in scored if s <= med + 0.25]

    def aggregate(
        self,
        client_updates: Mapping[str, WeightDict],
        *,
        use_byzantine_trim: bool = True,
    ) -> Dict[str, Any]:
        accepted_raw: List[Tuple[str, WeightDict, float]] = []
        rejected: List[Tuple[str, float]] = []

        for client_id, update in client_updates.items():
            sigma = self.score_update(update)
            self.client_stats[str(client_id)] = {
                "sigma": round(sigma, 6),
                "round": self.round,
                "accepted": bool(sigma < self.sigma_reject_threshold),
            }
            if sigma < self.sigma_reject_threshold:
                accepted_raw.append((str(client_id), update, sigma))
            else:
                rejected.append((str(client_id), sigma))

        pool = self._byzantine_trim(accepted_raw) if use_byzantine_trim else accepted_raw
        if not pool:
            self.last_aggregate = {
                "aggregated": False,
                "reason": "all updates rejected or trimmed",
                "round": self.round,
                "rejected": [{"client": c, "sigma": s} for c, s in rejected],
            }
            return self.last_aggregate

        weights = [1.0 / (s + self.weight_eps) for _, _, s in pool]
        tw = sum(weights)
        keys = list(pool[0][1].keys())
        aggregated: WeightDict = {}
        for key in keys:
            width = len(pool[0][1][key])
            aggregated[key] = []
            for j in range(width):
                acc = sum(
                    float(updates[1][key][j]) * weights[i] for i, updates in enumerate(pool)
                ) / tw
                aggregated[key].append(acc)

        if self.dp is not None and self.apply_dp_after_agg:
            aggregated = self.dp.clip_update(aggregated, max_norm=1.0)
            aggregated = self.dp.add_noise(aggregated, sensitivity=1.0)

        self.global_weights = aggregated
        self.round += 1

        self.last_aggregate = {
            "aggregated": True,
            "round": self.round,
            "accepted": len(pool),
            "rejected": len(rejected) + (len(accepted_raw) - len(pool)),
            "avg_sigma": round(sum(s for _, _, s in pool) / len(pool), 6),
            "rejected_clients": [c for c, _ in rejected],
            "trimmed_from_accepted": max(0, len(accepted_raw) - len(pool)),
            "dp": ({"epsilon": self.dp.epsilon, "delta": self.dp.delta} if self.dp else None),
        }
        return self.last_aggregate


def local_train_toy(
    global_w: WeightDict,
    *,
    poison: bool = False,
    honest_scale: float = 0.03,
) -> WeightDict:
    """Return a plausible post-local-training weight snapshot (toy)."""
    out = _deep_copy_weights(global_w)
    if poison:
        for k in out:
            out[k] = [float(x) + 80.0 for x in out[k]]
    else:
        for k in out:
            out[k] = [float(x) + random.gauss(0.0, honest_scale) for x in out[k]]
    return out


def client_self_check_gate(global_w: WeightDict, candidate: WeightDict) -> Tuple[float, Verdict]:
    """Map mean |Δ| to a σ-like stress in [0,1], run cognitive ``sigma_gate_core``."""
    mad = _mean_abs_delta(candidate, global_w)
    stress = float(min(1.0, mad / 10.0))
    st = SigmaState()
    sigma_update_q16(st, sigma_q16(stress), sigma_q16(0.9))
    return stress, sigma_gate(st)


@dataclass
class SigmaFederatedClient:
    client_id: str
    local_rows: int = 100
    use_self_gate: bool = True

    def train(
        self,
        global_weights: WeightDict,
        *,
        poison: bool = False,
        epochs: int = 1,
        lr: float = 0.01,
        honest_scale: float = 0.03,
        **_kw: Any,
    ) -> Optional[WeightDict]:
        _ = epochs, lr  # toy — unused
        cand = local_train_toy(global_weights, poison=poison, honest_scale=float(honest_scale))
        if not self.use_self_gate:
            return cand
        if poison:
            return cand
        _stress, v = client_self_check_gate(global_weights, cand)
        if v == Verdict.ABSTAIN:
            return None
        return cand


def run_mock_federation_lab(
    *,
    rounds: int = 3,
    workspace: str = "~/.cos/federation",
    include_poison: bool = True,
    use_byzantine: bool = True,
    n_clients: int = 10,
    sigma_threshold: float = 0.5,
) -> Dict[str, Any]:
    """Toy multi-round FL with poison nodes; persists ``fed_lab_state.json``. Used by ``cos federation --train``."""
    ws = Path(workspace).expanduser()
    ws.mkdir(parents=True, exist_ok=True)

    gw: WeightDict = {f"w{k}": [0.0, 0.0, 0.0] for k in range(4)}
    server = SigmaFederatedServer(
        gw, sigma_reject_threshold=sigma_threshold, dp=None
    )
    history: List[Dict[str, Any]] = []

    for _ in range(max(1, int(rounds))):
        updates: Dict[str, WeightDict] = {}
        for i in range(n_clients):
            cid = f"client-{i}"
            poison = bool(include_poison and i >= n_clients - 2)
            cl = SigmaFederatedClient(cid, use_self_gate=not poison)
            up = cl.train(server.global_weights, poison=poison)
            if up is not None:
                updates[cid] = up
        res = server.aggregate(updates, use_byzantine_trim=use_byzantine)
        history.append(res)

    state = {
        "rounds_run": len(history),
        "last_round": history[-1] if history else {},
        "history": history,
        "global_l2": round(_l2_norm(server.global_weights), 6),
        "note": "toy σ-screened FedAvg lab — not a security proof",
    }
    (ws / "fed_lab_state.json").write_text(
        json.dumps(state, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return state


def demo_aggregate_memory(
    *,
    include_poison: bool = True,
    use_byzantine: bool = True,
    dp: Optional[SigmaDifferentialPrivacy] = None,
) -> Dict[str, Any]:
    """Single-round in-memory demo for ``cos federated --aggregate`` (no HTTP)."""
    gw: WeightDict = {f"w{k}": [0.0, 0.0] for k in range(3)}
    server = SigmaFederatedServer(gw, sigma_reject_threshold=0.5, dp=dp)
    updates: Dict[str, WeightDict] = {}
    for i in range(10):
        poison = include_poison and i >= 8
        cl = SigmaFederatedClient(f"c{i}", use_self_gate=not poison)
        up = cl.train(server.global_weights, poison=poison)
        if up is not None:
            updates[f"c{i}"] = up
    agg = server.aggregate(updates, use_byzantine_trim=use_byzantine)
    return {"aggregate": agg, "client_stats": dict(server.client_stats)}


__all__ = [
    "SigmaDifferentialPrivacy",
    "SigmaFederatedClient",
    "SigmaFederatedServer",
    "WeightDict",
    "client_self_check_gate",
    "demo_aggregate_memory",
    "local_train_toy",
    "run_mock_federation_lab",
]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Federated σ-gate lab: local data stays on the client; only scalar-scored update metadata travels.

This is a **toy** policy shell (no Flower / PyTorch dependency). Updates are opaque placeholders
(string labels); the gate supplies ``sigma_avg`` for server-side accept vs reject. It does **not**
prove secure aggregation, privacy, or regulatory compliance — see ``docs/CLAIM_DISCIPLINE.md``.

:class:`FederatedNode` / :class:`FederatedSigma` add a second lab path: per-node σ histories and
scalar-only ``report()`` payloads for aggregation (again: not a guarantee of device-level secrecy).

Machine **unlearning** here means dropping listed local training pairs so later local rounds no
longer include them (lab hook for “forget this slice”, not certified model erasure).

``sigma_gate.h`` is **not** modified by this module.
"""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Optional, Sequence, Tuple, Union

from cos.sigma_gate import SigmaGate

__all__ = [
    "FederatedAggregator",
    "FederatedClient",
    "FederatedNode",
    "FederatedSigma",
]

DataPair = Tuple[str, str]
TrainBatch = Sequence[DataPair]

# Serialized update fields sent to the aggregator (no raw prompts/responses).
UpdateDict = Dict[str, Any]


class FederatedClient:
    """Local client: keeps ``(prompt, response)`` rows; emits σ-averaged update summaries only."""

    def __init__(self, client_id: str, gate: Optional[Any] = None) -> None:
        self.client_id = str(client_id)
        self.gate = gate if gate is not None else SigmaGate()
        self.local_data: List[DataPair] = []
        self.local_sigma_history: List[float] = []

    def train_local(self, data: TrainBatch) -> UpdateDict:
        """Score each local pair with σ-gate; return a compact update (no raw text leaves the client)."""
        batch = list(data)
        self.local_data.extend(batch)
        sigma_values: List[float] = []
        for prompt, response in batch:
            sigma, _verdict = self.gate.score(str(prompt), str(response))
            sigma_values.append(float(sigma))
        avg_sigma = sum(sigma_values) / float(max(len(sigma_values), 1))
        self.local_sigma_history.append(avg_sigma)
        return {
            "client_id": self.client_id,
            "n_samples": len(batch),
            "sigma_avg": round(avg_sigma, 4),
            "update": f"update_from_{self.client_id}",
        }

    def unlearn(self, data_to_forget: Union[TrainBatch, set]) -> Dict[str, Union[str, int]]:
        """Remove matching local pairs (same tuple identity as stored). Returns removal counts."""
        forget_set = set(data_to_forget)
        before = len(self.local_data)
        self.local_data = [d for d in self.local_data if d not in forget_set]
        removed = before - len(self.local_data)
        return {
            "client_id": self.client_id,
            "removed": removed,
            "remaining": len(self.local_data),
        }


class FederatedAggregator:
    """Server-side screen: reject updates whose ``sigma_avg`` exceeds ``max_sigma``."""

    def __init__(self, gate: Optional[Any] = None, max_sigma: float = 0.5) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.max_sigma = float(max_sigma)
        self.clients: Dict[str, FederatedClient] = {}
        self.rounds: List[Dict[str, Any]] = []

    def register_client(self, client: FederatedClient) -> None:
        self.clients[client.client_id] = client

    def aggregate(self, updates: Sequence[UpdateDict]) -> Dict[str, Any]:
        accepted: List[UpdateDict] = []
        rejected: List[UpdateDict] = []
        for update in updates:
            sigma_avg = float(update.get("sigma_avg", 1.0))
            if sigma_avg <= self.max_sigma:
                accepted.append(update)
            else:
                rejected.append(update)

        avg_sigma = round(
            sum(float(u["sigma_avg"]) for u in accepted) / float(max(len(accepted), 1)),
            4,
        )
        round_result: Dict[str, Any] = {
            "round": len(self.rounds) + 1,
            "total_updates": len(updates),
            "accepted": len(accepted),
            "rejected": len(rejected),
            "avg_sigma": avg_sigma,
            "rejected_clients": [str(r["client_id"]) for r in rejected],
        }
        self.rounds.append(round_result)
        return round_result

    def run_round(self, data_per_client: Mapping[str, TrainBatch]) -> Dict[str, Any]:
        """One federated round: each listed client trains locally; server aggregates summaries."""
        updates: List[UpdateDict] = []
        for client_id, data in data_per_client.items():
            client = self.clients.get(str(client_id))
            if client:
                updates.append(client.train_local(data))
        return self.aggregate(updates)

    def global_sigma(self) -> float:
        """Mean of per-round ``avg_sigma`` (0.5 if no rounds yet)."""
        if not self.rounds:
            return 0.5
        return round(
            sum(float(r["avg_sigma"]) for r in self.rounds) / float(len(self.rounds)),
            4,
        )


def _verdict_label(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    base = raw.split(".")[-1] if "." in raw else raw
    return base.upper()


class FederatedNode:
    """Lab node: scores ``(prompt, response)`` locally; uplink carries scalar summaries only."""

    def __init__(self, node_id: str, gate: Optional[Any] = None) -> None:
        self.node_id = str(node_id)
        self.gate = gate if gate is not None else SigmaGate()
        self.local_sigma_history: List[float] = []
        self.local_threshold: float = 0.3

    def measure(self, prompt: str, response: str) -> Dict[str, Any]:
        """Score locally; raw text is not exported by :meth:`report` (lab contract)."""
        σ, verdict = self.gate.score(str(prompt), str(response))
        σ = float(σ)
        self.local_sigma_history.append(σ)
        return {
            "node": self.node_id,
            "σ": round(σ, 4),
            "verdict": _verdict_label(verdict),
        }

    def report(self) -> Dict[str, Any]:
        """Return running-mean σ over the last window (no prompts/responses)."""
        if not self.local_sigma_history:
            return {"node": self.node_id, "avg_σ": 0.5, "n": 0}
        window = self.local_sigma_history[-100:]
        avg = sum(window) / float(len(window))
        return {
            "node": self.node_id,
            "avg_σ": round(float(avg), 4),
            "n": len(self.local_sigma_history),
        }

    def receive_global(self, global_threshold: float) -> None:
        self.local_threshold = float(global_threshold)


class FederatedSigma:
    """Toy aggregator: combines per-node scalar reports; optional trimmed-mean path."""

    def __init__(self) -> None:
        self.nodes: Dict[str, FederatedNode] = {}
        self.global_sigma: float = 0.5
        self.global_threshold: float = 0.3
        self.rounds: int = 0

    def add_node(self, node_id: str, gate: Optional[Any] = None) -> FederatedNode:
        node = FederatedNode(node_id, gate=gate)
        self.nodes[str(node_id)] = node
        return node

    def aggregate(self) -> Dict[str, Any]:
        reports: List[Dict[str, Any]] = []
        for node in self.nodes.values():
            report = node.report()
            if int(report["n"]) > 0:
                reports.append(report)

        if not reports:
            return {
                "global_σ": 0.5,
                "global_threshold": round(float(self.global_threshold), 4),
                "nodes": 0,
                "nodes_reporting": 0,
                "total_samples": 0,
                "round": self.rounds,
            }

        total_n = sum(int(r["n"]) for r in reports)
        weighted_σ = sum(float(r["avg_σ"]) * int(r["n"]) for r in reports) / float(max(total_n, 1))
        self.global_sigma = float(weighted_σ)
        self.rounds += 1
        self.global_threshold = min(0.5, float(weighted_σ) + 0.1)

        for node in self.nodes.values():
            node.receive_global(self.global_threshold)

        return {
            "global_σ": round(float(weighted_σ), 4),
            "global_threshold": round(float(self.global_threshold), 4),
            "nodes_reporting": len(reports),
            "total_samples": int(total_n),
            "round": self.rounds,
            "privacy_note": (
                "Lab API: aggregator methods shown here consume only scalar σ summaries from "
                "`FederatedNode.report`; they do not require raw prompts/responses. This does not "
                "by itself prove secure aggregation or legal compliance — see docs/CLAIM_DISCIPLINE.md."
            ),
        }

    def detect_outlier_nodes(self, deviation: float = 0.3) -> Dict[str, Any]:
        outliers: List[Dict[str, Any]] = []
        g = float(self.global_sigma)
        for node_id, node in self.nodes.items():
            report = node.report()
            if int(report["n"]) > 0:
                dev = abs(float(report["avg_σ"]) - g)
                if dev > float(deviation):
                    outliers.append(
                        {
                            "node": node_id,
                            "avg_σ": report["avg_σ"],
                            "deviation": round(float(dev), 4),
                            "status": "OUTLIER",
                        }
                    )
        return {
            "outliers": outliers,
            "n_outliers": len(outliers),
            "total_nodes": len(self.nodes),
        }

    def byzantine_robust_aggregate(self) -> Dict[str, Any]:
        reports: List[Dict[str, Any]] = []
        for node in self.nodes.values():
            rep = node.report()
            if int(rep["n"]) > 0:
                reports.append(rep)

        if len(reports) < 3:
            return self.aggregate()

        reports.sort(key=lambda r: float(r["avg_σ"]))
        trim = max(1, len(reports) // 5)
        trimmed = reports[trim:-trim] if len(reports) > 2 * trim else list(reports)

        total_n = sum(int(r["n"]) for r in trimmed)
        weighted_σ = (
            sum(float(r["avg_σ"]) * int(r["n"]) for r in trimmed) / float(max(total_n, 1))
        )
        self.global_sigma = float(weighted_σ)
        self.rounds += 1
        self.global_threshold = min(0.5, float(weighted_σ) + 0.1)
        for node in self.nodes.values():
            node.receive_global(self.global_threshold)

        return {
            "global_σ": round(float(weighted_σ), 4),
            "global_threshold": round(float(self.global_threshold), 4),
            "method": "trimmed_mean",
            "nodes_used": len(trimmed),
            "nodes_trimmed": len(reports) - len(trimmed),
            "round": self.rounds,
        }

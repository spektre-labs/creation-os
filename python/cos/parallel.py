# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-parallel — tensor / pipeline sharding plans and toy σ-per-shard aggregation (lab).

No distributed runtime: operators map these dicts to real frameworks. See
``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, Mapping, Sequence

__all__ = ["SigmaParallel"]


class SigmaParallel:
    """Sharding sketches + load-balance heuristics keyed off σ telemetry."""

    @staticmethod
    def tensor_parallel(model: Any, n_gpus: int) -> Dict[str, Any]:
        """Plan: split attention heads and FFN columns across ``n_gpus`` (``model`` opaque)."""
        n = max(1, int(n_gpus))
        return {
            "mode": "tensor_parallel",
            "n_gpus": n,
            "heads_per_shard": f"ceil(n_heads/{n})",
            "ffn_shards": n,
            "model_present": model is not None,
        }

    @staticmethod
    def pipeline_parallel(model: Any, n_gpus: int) -> Dict[str, Any]:
        """Plan: layer bands per GPU (``model`` opaque)."""
        n = max(1, int(n_gpus))
        return {
            "mode": "pipeline_parallel",
            "n_gpus": n,
            "stages": n,
            "microbatch_note": "Use pipeline bubbles / deferred batching per driver docs.",
            "model_present": model is not None,
        }

    @staticmethod
    def sigma_per_shard(shard_sigmas: Sequence[float]) -> Dict[str, Any]:
        """Aggregate per-GPU σ proxies (max stress + mean for dashboards)."""
        xs = [float(x) for x in shard_sigmas]
        if not xs:
            return {"aggregate": 0.5, "max": 0.5, "mean": 0.5, "n_shards": 0}
        return {
            "aggregate": float(max(xs)),
            "max": float(max(xs)),
            "mean": float(sum(xs) / len(xs)),
            "n_shards": len(xs),
        }

    @staticmethod
    def load_balance(shards: Sequence[Mapping[str, Any]], gate: Any) -> Dict[str, Any]:
        """Re-rank shards by σ-derived pressure (lower σ ⇒ more headroom). Tosks use ``load`` and ``sigma`` keys."""
        del gate  # reserved for fleet hooks
        ranked = sorted(
            (dict(s) for s in shards),
            key=lambda d: float(d.get("sigma", 0.5)) * (1.0 + float(d.get("load", 1.0))),
        )
        return {"order": ranked, "prefer": ranked[0] if ranked else {}}

    @staticmethod
    def communication_overhead(topology: Mapping[str, Any]) -> Dict[str, Any]:
        """Rough NVLink / PCIe penalty model from edge list + bytes (toy)."""
        edges = int(topology.get("edges", 0))
        bytes_per_step = float(topology.get("bytes_per_step", 1e6))
        gbps = float(topology.get("link_gbps", 50.0))
        seconds = (bytes_per_step * max(0, edges)) / max(gbps * 1e9, 1.0)
        return {
            "estimated_sync_ms": round(seconds * 1000.0, 4),
            "edges": edges,
            "note": "Calibrated only for relative comparisons in lab.",
        }

    @staticmethod
    def hybrid_parallel(model: Any, n_tensor: int, n_pipeline: int) -> Dict[str, Any]:
        """TP within a node × PP across nodes (config blob for orch)."""
        nt, np = max(1, int(n_tensor)), max(1, int(n_pipeline))
        return {
            "mode": "hybrid_parallel",
            "tensor": SigmaParallel.tensor_parallel(model, nt),
            "pipeline": SigmaParallel.pipeline_parallel(model, np),
            "world_size": nt * np,
            "arrangement": f"{np} pipeline stages × {nt} tensor ranks / stage",
        }

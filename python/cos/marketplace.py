# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-marketplace — in-memory probe / model profile registry (lab).

No live marketplace network. SHA-256 verifies payload bytes only. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import hashlib
import json
import uuid
from typing import Any, Dict, List, Mapping, Optional, Sequence

__all__ = ["SigmaMarketplace"]


class SigmaMarketplace:
    """Publish/search probes and model profiles; leaderboard sorted by σ (lower better)."""

    def __init__(self) -> None:
        self._probes: Dict[str, Dict[str, Any]] = {}
        self._models: Dict[str, Dict[str, Any]] = {}
        self._community_benchmarks: List[Dict[str, Any]] = []

    def publish_probe(self, probe: Mapping[str, Any], metadata: Mapping[str, Any], benchmark_results: Mapping[str, Any]) -> Dict[str, Any]:
        pid = str(uuid.uuid4())[:12]
        blob = json.dumps({"probe": dict(probe), "metadata": dict(metadata), "bench": dict(benchmark_results)}, sort_keys=True, default=str)
        digest = hashlib.sha256(blob.encode("utf-8")).hexdigest()
        self._probes[pid] = {"probe": dict(probe), "metadata": dict(metadata), "benchmark_results": dict(benchmark_results), "sha256": digest}
        return {"probe_id": pid, "sha256": digest}

    def search_probes(self, task: str, model: str) -> List[Dict[str, Any]]:
        q = f"{task} {model}".lower()
        hits: List[Dict[str, Any]] = []
        for pid, row in self._probes.items():
            hay = json.dumps(row, default=str).lower()
            if q in hay or any(tok in hay for tok in q.split()):
                hits.append({"probe_id": pid, **row})
        return hits

    def download_probe(self, probe_id: str, *, expected_sha256: Optional[str] = None) -> Dict[str, Any]:
        row = self._probes.get(str(probe_id))
        if not row:
            return {"ok": False, "error": "not_found"}
        blob = json.dumps({"probe": row["probe"], "metadata": row["metadata"], "bench": row["benchmark_results"]}, sort_keys=True, default=str)
        digest = hashlib.sha256(blob.encode("utf-8")).hexdigest()
        if digest != row["sha256"]:
            return {"ok": False, "error": "internal_checksum_mismatch"}
        if expected_sha256 and expected_sha256.strip() != digest:
            return {"ok": False, "error": "sha256_mismatch"}
        return {"ok": True, "payload": row, "sha256": digest}

    def publish_model_profile(self, model: Mapping[str, Any], sigma_benchmark: Mapping[str, Any]) -> Dict[str, Any]:
        mid = str(model.get("id") or uuid.uuid4())[:16]
        self._models[mid] = {"model": dict(model), "sigma_benchmark": dict(sigma_benchmark)}
        return {"model_id": mid}

    def leaderboard(self) -> List[Dict[str, Any]]:
        rows = []
        for mid, row in self._models.items():
            sb = row.get("sigma_benchmark") or {}
            sigma = float(sb.get("sigma", sb.get("mean_sigma", 1.0)))
            rows.append({"model_id": mid, "sigma": sigma, "benchmark": sb.get("name", "unknown")})
        rows.sort(key=lambda x: x["sigma"])
        return rows

    def add_community_benchmark(self, name: str, rows: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        self._community_benchmarks.append({"name": str(name), "rows": [dict(r) for r in rows]})
        return {"registered": True, "n": len(self._community_benchmarks)}

    @staticmethod
    def sigma_score_as_badge(max_sigma: float) -> str:
        s = float(max_sigma)
        if s < 0.2:
            return "Spektre Verified σ<0.2 (lab badge — not third-party audit)"
        return f"Spektre σ={round(s, 3)} (lab badge)"

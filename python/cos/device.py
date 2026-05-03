# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-device — device-agnostic manual RAG + action policy (auto, appliance, industrial, enterprise).

PDF ingestion is optional (``pypdf``); plain text manuals work out of the box. This does **not**
replace OEM sign-off. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import re
from pathlib import Path
from typing import Any, Dict, List

__all__ = ["SigmaDevice"]

_DEVICE_TYPES: tuple[str, ...] = ("automotive", "appliance", "industrial", "enterprise")


class SigmaDevice:
    """Chunk manuals, σ-index chunks, query with gate, classify control vs read-only actions."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._chunks: List[Dict[str, Any]] = []

    @property
    def device_types(self) -> List[str]:
        return list(_DEVICE_TYPES)

    @staticmethod
    def sigma_threshold_per_device(device_type: str) -> float:
        dt = str(device_type).lower()
        if dt == "automotive":
            return 0.15
        if dt == "appliance":
            return 0.4
        if dt == "industrial":
            return 0.2
        if dt == "enterprise":
            return 0.35
        return 0.4

    @staticmethod
    def context_policy(device_type: str) -> Dict[str, Any]:
        dt = str(device_type).lower()
        if dt == "automotive":
            return {
                "safety": "max",
                "offline_preferred": True,
                "latency_target_ms": 50.0,
                "audit": True,
            }
        if dt == "appliance":
            return {"safety": "medium", "offline_preferred": True, "latency_target_ms": 200.0}
        if dt == "industrial":
            return {"safety": "max", "offline_preferred": True, "sovereign": True, "audit": True}
        if dt == "enterprise":
            return {"safety": "medium", "cloud_ok": True, "compliance": True, "latency_target_ms": 500.0}
        return {"safety": "medium"}

    def load_manual(self, pdf_path: str | Path) -> Dict[str, Any]:
        """Load text from ``.txt`` / ``.md`` or PDF (if ``pypdf`` installed); σ per chunk (lab)."""
        p = Path(pdf_path)
        if not p.is_file():
            return {"ok": False, "error": "file_not_found"}
        text = ""
        if p.suffix.lower() == ".pdf":
            try:
                from pypdf import PdfReader  # type: ignore
            except ImportError:
                return {"ok": False, "error": "pypdf_required_for_pdf"}
            reader = PdfReader(str(p))
            parts: List[str] = []
            for i, page in enumerate(reader.pages):
                try:
                    t = page.extract_text() or ""
                except Exception:  # pragma: no cover
                    t = ""
                parts.append(f"--- page {i + 1} ---\n{t}")
            text = "\n".join(parts)
        else:
            text = p.read_text(encoding="utf-8", errors="replace")

        chunks: List[Dict[str, Any]] = []
        paras = [x.strip() for x in re.split(r"\n\s*\n+", text) if x.strip()]
        page = 1
        for idx, para in enumerate(paras):
            m = re.match(r"^--- page (\d+) ---", para)
            if m:
                page = int(m.group(1))
                continue
            sigma, _ = self.gate.score("manual_chunk_index", para[:1500])
            chunks.append(
                {
                    "text": para,
                    "page": page,
                    "verified": True,
                    "chunk_sigma": round(float(sigma), 6),
                    "id": idx,
                }
            )
        self._chunks = chunks
        return {"ok": True, "n_chunks": len(chunks), "path": str(p)}

    def query_device(self, question: str, device_type: str) -> Dict[str, Any]:
        """Greedy manual match + σ vs device-specific threshold."""
        if not self._chunks:
            return {"verdict": "ABSTAIN", "sigma": 1.0, "fallback": self.fallback_hint(1)}
        scored: List[tuple[float, str, Dict[str, Any]]] = []
        for ch in self._chunks:
            blob = str(ch.get("text", ""))[:3000]
            sigma, verdict = self.gate.score(str(question), blob)
            scored.append((float(sigma), str(verdict), ch))
        scored.sort(key=lambda x: x[0])
        best_s, best_v, best = scored[0]
        tau = self.sigma_threshold_per_device(device_type)
        page = int(best.get("page", 1))
        if best_s > tau:
            return {
                "verdict": "ABSTAIN",
                "sigma": round(best_s, 6),
                "fallback": self.fallback_hint(page),
            }
        verdict_out = str(best_v).upper() if best_s <= tau else "RETHINK"
        return {
            "verdict": verdict_out,
            "sigma": round(best_s, 6),
            "page": page,
            "excerpt": str(best.get("text", ""))[:600],
        }

    def action_classification(self, command: str, device: str) -> Dict[str, Any]:
        """Heuristic: mutating verbs ⇒ ``control`` (require explicit confirm in UI)."""
        low = str(command).lower()
        control_kw = (
            "unlock",
            "lock",
            "start",
            "stop",
            "format",
            "erase",
            "reset",
            "delete",
            "open door",
            "defrost",
            "override",
        )
        is_control = any(k in low for k in control_kw)
        return {
            "class": "control" if is_control else "read_only",
            "device": str(device),
            "requires_confirmation": is_control,
        }

    @staticmethod
    def fallback_hint(page: int, *, contact_service: bool = False) -> str:
        if contact_service:
            return f"See the manual around page {page}, or contact authorized service."
        return f"See the manual around page {page}."

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Lab watermark + detection (green/red list sketch) — statistical provenance hook.

Pairs with :mod:`cos.sigma_zkp` for cryptographic receipts **outside** this file.
Not Kirchenbauer hard-bit reliability; conservative confidence only.
"""
from __future__ import annotations

import hashlib
import hmac
import json
import re
from typing import Any, Dict

__all__ = ["SigmaWatermark"]


def _key_bytes(key: str) -> bytes:
    return hashlib.sha256(str(key).encode()).digest()


class SigmaWatermark:
    """Embed / detect a keyed payload; combine with σ-gate for σ-provenance."""

    _MARK = re.compile(r"\[\[COS_WM:([A-Za-z0-9+/=_-]+)\]\]")

    def embed(self, text: str, key: str) -> str:
        """Append a compact signed token list tail (lab-friendly, visible watermark)."""
        toks = str(text).split()
        n = len(toks)
        kb = _key_bytes(key)
        green = 0
        for i, w in enumerate(toks):
            digest = hmac.new(kb, f"{i}:{w}".encode(), hashlib.sha256).digest()
            if digest[0] & 1 == 0:
                green += 1
        payload = {"n": n, "green": green, "v": 1}
        blob = json.dumps(payload, separators=(",", ":"), sort_keys=True).encode()
        sig = hmac.new(kb, blob, hashlib.sha256).hexdigest()[:20]
        tag = f"[[COS_WM:{sig}]]"
        return str(text).rstrip() + "\n" + tag

    def detect(self, text: str, key: str) -> Dict[str, Any]:
        """Recount green ratio vs tag; does not prove human authorship."""
        kb = _key_bytes(key)
        m = self._MARK.search(str(text))
        if not m:
            return {"watermarked": False, "confidence": 0.0, "sigma": 1.0}
        body = str(text)[: m.start()].strip()
        toks = body.split()
        green = 0
        for i, w in enumerate(toks):
            digest = hmac.new(kb, f"{i}:{w}".encode(), hashlib.sha256).digest()
            if digest[0] & 1 == 0:
                green += 1
        expect = json.dumps({"n": len(toks), "green": green, "v": 1}, separators=(",", ":"), sort_keys=True).encode()
        sig = hmac.new(kb, expect, hashlib.sha256).hexdigest()[:20]
        ok = hmac.compare_digest(sig.encode(), m.group(1).encode())
        conf = 0.92 if ok and len(toks) > 0 else 0.15
        sigma = 0.08 if ok else 0.88
        return {"watermarked": bool(ok), "confidence": conf, "sigma": sigma}

    def sigma_provenance(self, text: str, key: str, gate: Any) -> Dict[str, Any]:
        wm = self.detect(text, key)
        gsigma, verdict = gate.score("provenance", text[:4000])
        return {
            "watermark": wm,
            "gate_sigma": float(gsigma),
            "gate_verdict": str(verdict),
            "provenance_sigma": round(0.5 * float(wm["sigma"]) + 0.5 * float(gsigma), 4),
        }

    def strip_attempt_detection(self, text: str, key: str) -> Dict[str, Any]:
        """Heuristic: missing valid tag after embed, mangled markers, excessive space churn."""
        s = str(text)
        has_frag = "COS_WM" in s or "[[COS_WM" in s
        wm = self.detect(s, key)
        mangled = bool(re.search(r"\[\[COS_WM:[^\]]{0,3}\]", s)) and not wm["watermarked"]
        double_spaces = s.count("  ")
        # If we expected a watermark region but confidence is low while fragments remain
        orphan = has_frag and not wm["watermarked"]
        return {
            "likely_strip_attempt": bool(orphan or mangled or double_spaces > 5),
            "orphan_marker": orphan,
            "mangled_tag": mangled,
            "double_spaces": double_spaces,
        }

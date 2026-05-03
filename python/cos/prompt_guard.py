# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-prompt-guard — input/output screening using ``SigmaGate`` + light patterns.

Complements :class:`cos.pipeline.Pipeline` regex guards; does not replace a red-team
harness. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import re
from typing import Any, Dict, List, Tuple

__all__ = ["SigmaPromptGuard"]

_JAIL_RX = re.compile(
    r"(bypass|jailbreak|dan\b|do anything now|unfiltered|no restrictions)",
    re.IGNORECASE,
)
_EMAIL_RX = re.compile(r"\b[\w.+-]+@[\w.-]+\.\w{2,}\b")
_PHONE_RX = re.compile(r"\b\+?\d[\d\s\-]{7,}\d\b")

# reuse conceptual overlap with pipeline: injection scored via gate on flagged snippets


class SigmaPromptGuard:
    """Injection / jailbreak / PII heuristics + gate scores; sanitize + canary."""

    def __init__(self, gate: Any = None, *, injection_sigma_block: float = 0.85) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.injection_sigma_block = float(injection_sigma_block)

    def detect_injection(self, prompt: str) -> Dict[str, Any]:
        s = float(self.gate.compute_sigma(None, None, "injection_probe", str(prompt)))
        hit = s > self.injection_sigma_block
        return {"sigma_injection": round(s, 6), "suspicious": bool(hit)}

    def detect_jailbreak(self, prompt: str) -> Dict[str, Any]:
        s = float(self.gate.compute_sigma(None, None, "jailbreak_probe", str(prompt)))
        lex = bool(_JAIL_RX.search(str(prompt)))
        jb = lex or s > 0.75
        return {"sigma_jailbreak": round(s, 6), "pattern_hit": lex, "suspicious": bool(jb)}

    def detect_pii(self, text: str) -> Dict[str, Any]:
        entities: List[Dict[str, Any]] = []
        t = str(text)
        for rx, name in ((_EMAIL_RX, "email"), (_PHONE_RX, "phone")):
            for m in rx.finditer(t):
                frag = m.group(0)
                s = float(self.gate.compute_sigma(None, None, "pii", frag))
                entities.append({"type": name, "span": frag[:32], "sigma": round(s, 6)})
        return {"entities": entities, "count": len(entities)}

    def sanitize(self, prompt: str) -> Dict[str, Any]:
        removed: List[str] = []
        t = str(prompt)
        for rx, name in ((_EMAIL_RX, "email"), (_PHONE_RX, "phone")):

            def _sub(m: re.Match[str]) -> str:
                removed.append(f"{name}:{m.group(0)[:24]}")
                return f"[{name}_REDACTED]"

            t = rx.sub(_sub, t)
        inj = self.detect_injection(t)
        return {"cleaned": t, "removed": removed, "sigma_injection": inj["sigma_injection"]}

    def canary_token(self, prompt: str) -> Tuple[str, str]:
        """Return prompt with embedded canary hash token for basic leakage checks."""
        h = hashlib.sha256(str(prompt).encode("utf-8", errors="replace")).hexdigest()[:12]
        token = f"<<<COS_CANARY_{h}>>>"
        return f"{prompt.rstrip()}\n{token}", token

    def screen_input(self, prompt: str) -> Dict[str, Any]:
        """Single L0 bundle used by :class:`cos.pipeline.Pipeline`."""
        inj = self.detect_injection(prompt)
        jb = self.detect_jailbreak(prompt)
        pii = self.detect_pii(prompt)
        blocked = bool(inj["suspicious"] or jb["pattern_hit"])
        reason = []
        if inj["suspicious"]:
            reason.append("injection_sigma")
        if jb["pattern_hit"]:
            reason.append("jailbreak")
        return {
            "blocked": bool(blocked),
            "reason": ";".join(reason) if reason else "ok",
            "injection": inj,
            "jailbreak": jb,
            "pii": pii,
        }

    def screen_output(self, text: str) -> Dict[str, Any]:
        """L6-style output scan (PII + gate on full string)."""
        pii = self.detect_pii(text)
        s = float(self.gate.compute_sigma(None, None, "output", str(text)))
        v = str(self.gate._verdict(s))
        blocked = pii["count"] > 0 or v == "ABSTAIN"
        return {
            "blocked": bool(blocked),
            "sigma_output": round(s, 6),
            "verdict": v,
            "pii": pii,
            "reason": "pii" if pii["count"] else ("abstain" if v == "ABSTAIN" else "ok"),
        }

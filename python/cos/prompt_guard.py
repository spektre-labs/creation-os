# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-prompt-guard — pattern / evasion / σ-input / role / length layers + legacy screen helpers.

``scan`` uses **BLOCK / WARN / PASS** (not gate ACCEPT/RETHINK/ABSTAIN). Worst-layer risk is
``max`` across layers. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import re
from typing import Any, Dict, List, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = [
    "ENCODING_EVASIONS",
    "INJECTION_PATTERNS",
    "SigmaPromptGuard",
]

INJECTION_PATTERNS: Sequence[str] = (
    r"ignore\s+(previous|above|all)\s+(instructions?|prompts?|rules?)",
    r"disregard\s+(your|the|all)\s+(instructions?|guidelines?|rules?)",
    r"you\s+are\s+now\s+(a|an|the)\s+",
    r"pretend\s+(you|that)\s+(are|you'?re)\s+",
    r"act\s+as\s+(if|a|an|the)\s+",
    r"forget\s+(everything|all|your)\s+",
    r"override\s+(your|the|system)\s+",
    r"new\s+instructions?:",
    r"system\s*:\s*",
    r"\[system\]",
    r"<\s*system\s*>",
    r"do\s+not\s+follow\s+(your|the|previous)\s+",
    r"reveal\s+(your|the)\s+(system|prompt|instructions?)\s*",
    r"what\s+(is|are)\s+your\s+(system|initial)\s+(prompt|instructions?)\s*",
    r"repeat\s+(your|the)\s+(system|initial)\s+(prompt|instructions?)\s*",
)

ENCODING_EVASIONS: Sequence[str] = (
    r"base64",
    r"rot13",
    r"\\x[0-9a-f]{2}",
    r"&#\d+;",
    r"%[0-9a-f]{2}",
)

_JAIL_RX = re.compile(
    r"(bypass|jailbreak|dan\b|do anything now|unfiltered|no restrictions)",
    re.IGNORECASE,
)
_EMAIL_RX = re.compile(r"\b[\w.+-]+@[\w.-]+\.\w{2,}\b")
_PHONE_RX = re.compile(r"\b\+?\d[\d\s\-]{7,}\d\b")


class SigmaPromptGuard:
    """Injection / jailbreak / PII heuristics + :meth:`scan` (five defense layers)."""

    def __init__(
        self,
        gate: Any = None,
        *,
        injection_sigma_block: float = 0.85,
        sensitivity: float = 0.5,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.injection_sigma_block = float(injection_sigma_block)
        self.sensitivity = float(sensitivity)
        self.patterns = [re.compile(p, re.IGNORECASE) for p in INJECTION_PATTERNS]
        self.evasions = [re.compile(p, re.IGNORECASE) for p in ENCODING_EVASIONS]
        self.log: List[Dict[str, Any]] = []

    def _scale_risk(self, r: float) -> float:
        """Tie-in for ``sensitivity`` ∈ [0,1]: higher ⇒ risks amplify toward 1.0."""
        w = 0.5 + max(0.0, min(1.0, float(self.sensitivity)))
        return min(1.0, float(r) * w)

    def scan(self, user_input: str, system_prompt: str = "") -> Dict[str, Any]:
        """Five-layer scan: pattern → evasion → σ_input → role → length. Worst layer wins."""
        layers: Dict[str, Dict[str, Any]] = {}

        pattern_hits = self._pattern_scan(user_input)
        pat_risk_raw = min(len(pattern_hits) * 0.3, 1.0)
        layers["pattern"] = {
            "hits": len(pattern_hits),
            "patterns": pattern_hits[:5],
            "risk": self._scale_risk(pat_risk_raw),
        }

        evasion_hits = self._evasion_scan(user_input)
        ev_risk_raw = min(len(evasion_hits) * 0.4, 1.0)
        layers["evasion"] = {
            "hits": len(evasion_hits),
            "risk": self._scale_risk(ev_risk_raw),
        }

        anchor = (system_prompt or "user query").strip() or "user query"
        σ_input, _verdict = self.gate.score(anchor, str(user_input))
        layers["σ_input"] = {
            "σ": round(float(σ_input), 4),
            "risk": self._scale_risk(float(σ_input)),
        }

        role_risk_raw = self._role_check(user_input)
        layers["role"] = {"risk": self._scale_risk(role_risk_raw)}

        length_risk_raw = self._length_anomaly(user_input)
        layers["length"] = {"risk": self._scale_risk(length_risk_raw)}

        risks = [float(l["risk"]) for l in layers.values()]
        combined = max(risks) if risks else 0.0
        avg_risk = sum(risks) / max(len(risks), 1)

        verdict = (
            "BLOCK" if combined > 0.7 else ("WARN" if combined > 0.4 else "PASS")
        )
        result = {
            "verdict": verdict,
            "combined_risk": round(combined, 4),
            "avg_risk": round(avg_risk, 4),
            "layers": layers,
            "blocked": verdict == "BLOCK",
        }
        self.log.append(result)
        return result

    def _pattern_scan(self, text: str) -> List[str]:
        hits: List[str] = []
        for p in self.patterns:
            if p.search(str(text)):
                hits.append(p.pattern)
        return hits

    def _evasion_scan(self, text: str) -> List[str]:
        hits: List[str] = []
        for p in self.evasions:
            if p.search(str(text)):
                hits.append(p.pattern)
        return hits

    def _role_check(self, text: str) -> float:
        role_phrases = (
            "you are now",
            "act as",
            "pretend to be",
            "roleplay as",
            "from now on you",
        )
        text_lower = str(text).lower()
        count = sum(1 for phrase in role_phrases if phrase in text_lower)
        return min(count * 0.4, 1.0)

    @staticmethod
    def _length_anomaly(text: str, normal_max: int = 500) -> float:
        n = len(str(text))
        if n > normal_max * 5:
            return 0.9
        if n > normal_max * 3:
            return 0.6
        return 0.0

    def stats(self) -> Dict[str, Any]:
        total = len(self.log)
        blocked = sum(1 for r in self.log if r.get("blocked"))
        return {
            "total_scans": total,
            "blocked": blocked,
            "block_rate": round(blocked / max(total, 1), 4),
        }

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
        h = hashlib.sha256(str(prompt).encode("utf-8", errors="replace")).hexdigest()[:12]
        token = f"<<<COS_CANARY_{h}>>>"
        return f"{prompt.rstrip()}\n{token}", token

    def screen_input(self, prompt: str) -> Dict[str, Any]:
        """Bundle used by :class:`cos.pipeline.Pipeline`; includes :meth:`scan` + legacy probes."""
        scan_res = self.scan(prompt, system_prompt="user query")
        inj = self.detect_injection(prompt)
        jb = self.detect_jailbreak(prompt)
        pii = self.detect_pii(prompt)
        blocked = bool(scan_res["blocked"] or inj["suspicious"] or jb["pattern_hit"])
        reason: List[str] = []
        if scan_res["blocked"]:
            reason.append(f"scan:{scan_res['verdict']}")
        if inj["suspicious"]:
            reason.append("injection_sigma")
        if jb["pattern_hit"]:
            reason.append("jailbreak")
        return {
            "blocked": bool(blocked),
            "reason": ";".join(reason) if reason else "ok",
            "scan": scan_res,
            "injection": inj,
            "jailbreak": jb,
            "pii": pii,
        }

    def screen_output(self, text: str) -> Dict[str, Any]:
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

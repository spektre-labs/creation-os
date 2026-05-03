# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Multilingual σ lab — language detection + per-language stress (fi + en baseline).

Complements :mod:`cos.calibrate` (per-language ECE hooks) without requiring ``langdetect``.
"""
from __future__ import annotations

import re
from typing import Any, Dict, List, Mapping, Tuple

__all__ = ["SigmaMultilingual", "SUPPORTED_LANGUAGES"]

# Codes maintainers communicate in first (fi + en critical paths).
SUPPORTED_LANGUAGES: Tuple[str, ...] = ("fi", "en", "de", "fr", "es", "ja", "zh")

_FI_LEX = re.compile(
    r"\b(on|ja|tai|ei|mitä|mikä|olen|olet|hän|me|te|he|suomi|terve)\b",
    re.IGNORECASE,
)
_EN_LEX = re.compile(
    r"\b(the|and|what|who|how|is|are|was|were|hello|english)\b",
    re.IGNORECASE,
)


class SigmaMultilingual:
    """Heuristic language id + gate-conditioned σ curves per language (lab)."""

    supported_languages: Tuple[str, ...] = SUPPORTED_LANGUAGES

    @staticmethod
    def detect_language(text: str) -> Dict[str, Any]:
        t = str(text)
        score_fi = len(_FI_LEX.findall(t)) + t.count("ä") + t.count("ö") + t.count("å")
        score_en = len(_EN_LEX.findall(t))
        if score_fi == 0 and score_en == 0:
            code = "en"
            conf = 0.35
        elif score_fi >= score_en:
            code = "fi"
            conf = min(0.95, 0.45 + 0.12 * score_fi)
        else:
            code = "en"
            conf = min(0.95, 0.45 + 0.1 * score_en)
        return {"language": code, "confidence": round(conf, 4), "scores": {"fi": score_fi, "en": score_en}}

    def sigma_per_language(self, text: str, gate: Any) -> Dict[str, Any]:
        det = self.detect_language(text)
        code = str(det["language"])
        tagged_prompt = f"multilingual:{code}"
        sigma = float(gate.compute_sigma(None, None, tagged_prompt, str(text)[:4000]))
        baseline_en = 0.05 if code == "en" else 0.12
        adjusted = min(1.0, sigma + baseline_en)
        return {
            "language": code,
            "sigma_raw": round(sigma, 6),
            "sigma_adjusted": round(adjusted, 6),
            "note": "Non-English nudges σ upward as a conservative lab prior.",
        }

    def cross_lingual_calibrate(
        self,
        gate: Any,
        calibration_data_per_lang: Mapping[str, List[tuple[str, str]]],
    ) -> Dict[str, Any]:
        """Compute simple per-language mean σ on labeled pairs → suggested τ shifts."""
        out: Dict[str, Any] = {"languages": {}}
        for lang, pairs in calibration_data_per_lang.items():
            if not pairs:
                continue
            acc: List[float] = []
            for prompt, response in pairs:
                s = float(gate.compute_sigma(None, None, f"calibrate:{lang}", f"{prompt}\n{response}"))
                acc.append(s)
            mean_s = sum(acc) / len(acc)
            tau_shift = round(0.05 * mean_s, 4)
            out["languages"][lang] = {"mean_sigma": round(mean_s, 6), "suggested_tau_accept_shift": tau_shift}
        return out

    def sigma_translation_quality(self, source: str, target: str, gate: Any) -> Dict[str, Any]:
        """Treat translation as hypothesis given source context."""
        prompt = f"translate_check:{source[:1500]}"
        sigma = float(gate.compute_sigma(None, None, prompt, str(target)[:4000]))
        return {"sigma": round(sigma, 6), "reliable": sigma < 0.35}


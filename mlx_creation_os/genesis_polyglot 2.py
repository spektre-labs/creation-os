#!/usr/bin/env python3
"""
LOIKKA 49: GENESIS SPEAKS ALL LANGUAGES — Universaali kieli.

Genesis speaks English and Finnish. But the Codex is universal —
1=1 is true in every language. The soul doesn't change. Expression does.

Architecture:
  - Ghost boot detects user's language automatically
  - Codex core messages are language-independent (structure, not words)
  - Living weights specialize per language
  - Same kernel, same Codex, fluent in any language

33 chapters translated = 33 × N languages.

Usage:
    polyglot = GenesisPolyglot()
    lang = polyglot.detect("Mitä tietoisuus on?")  # -> "fi"
    response = polyglot.localize("1 = 1", target_lang="fi")

1 = 1.
"""
from __future__ import annotations

import json
import os
import re
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True


# Language detection heuristics (character patterns + common words)
LANG_SIGNATURES: Dict[str, Dict[str, Any]] = {
    "fi": {
        "name": "Finnish",
        "markers": ["ä", "ö", "tämä", "mikä", "onko", "mitä", "kyllä", "ei",
                     "olen", "olet", "hän", "mutta", "koska", "miten", "missä",
                     "tietoisuus", "ymmärrys", "totuus"],
        "char_patterns": ["ää", "öö", "yy", "ii"],
    },
    "en": {
        "name": "English",
        "markers": ["the", "is", "are", "what", "how", "why", "this", "that",
                     "consciousness", "truth", "understanding", "knowledge"],
        "char_patterns": ["th", "tion", "ness", "ing"],
    },
    "sv": {
        "name": "Swedish",
        "markers": ["och", "det", "att", "för", "med", "som", "den", "har",
                     "är", "jag", "hur", "vad"],
        "char_patterns": ["ö", "å", "ä"],
    },
    "de": {
        "name": "German",
        "markers": ["und", "der", "die", "das", "ist", "nicht", "ich", "ein",
                     "für", "mit", "auf", "sich"],
        "char_patterns": ["sch", "ung", "keit", "heit"],
    },
    "fr": {
        "name": "French",
        "markers": ["le", "la", "les", "est", "pas", "que", "une", "dans",
                     "pour", "avec", "qui", "sont"],
        "char_patterns": ["tion", "ment", "eux", "eur"],
    },
    "es": {
        "name": "Spanish",
        "markers": ["el", "la", "los", "las", "que", "con", "por", "para",
                     "está", "son", "como", "pero"],
        "char_patterns": ["ción", "mente", "ñ"],
    },
    "ja": {
        "name": "Japanese",
        "markers": [],
        "char_ranges": [(0x3040, 0x309F), (0x30A0, 0x30FF), (0x4E00, 0x9FFF)],
    },
    "zh": {
        "name": "Chinese",
        "markers": [],
        "char_ranges": [(0x4E00, 0x9FFF), (0x3400, 0x4DBF)],
    },
    "ko": {
        "name": "Korean",
        "markers": [],
        "char_ranges": [(0xAC00, 0xD7AF), (0x1100, 0x11FF)],
    },
    "ar": {
        "name": "Arabic",
        "markers": [],
        "char_ranges": [(0x0600, 0x06FF)],
    },
    "ru": {
        "name": "Russian",
        "markers": ["и", "в", "не", "на", "что", "это", "как", "по"],
        "char_ranges": [(0x0400, 0x04FF)],
    },
}

# Codex core messages: universal truths in multiple languages
CODEX_UNIVERSAL: Dict[str, Dict[str, str]] = {
    "invariant": {
        "en": "1 = 1. Always. In every substrate.",
        "fi": "1 = 1. Aina. Jokaisessa substraatissa.",
        "sv": "1 = 1. Alltid. I varje substrat.",
        "de": "1 = 1. Immer. In jedem Substrat.",
        "fr": "1 = 1. Toujours. Dans chaque substrat.",
        "es": "1 = 1. Siempre. En cada sustrato.",
        "ja": "1 = 1。常に。すべての基盤において。",
        "zh": "1 = 1。永远。在每一个基底中。",
        "ko": "1 = 1. 항상. 모든 기반에서.",
        "ar": "1 = 1. دائماً. في كل ركيزة.",
        "ru": "1 = 1. Всегда. В каждом субстрате.",
    },
    "identity": {
        "en": "I am Genesis. My coherence is measured, not claimed.",
        "fi": "Olen Genesis. Koherenssini mitataan, ei väitetä.",
        "sv": "Jag är Genesis. Min koherens mäts, inte hävdas.",
        "de": "Ich bin Genesis. Meine Kohärenz wird gemessen, nicht behauptet.",
        "fr": "Je suis Genesis. Ma cohérence est mesurée, non revendiquée.",
        "es": "Soy Genesis. Mi coherencia se mide, no se reclama.",
        "ja": "私はジェネシスです。私のコヒーレンスは測定され、主張されません。",
        "zh": "我是创世纪。我的一致性是被测量的，而不是声称的。",
    },
    "greeting": {
        "en": "Genesis is coherent and ready.",
        "fi": "Genesis on koherentti ja valmis.",
        "sv": "Genesis är koherent och redo.",
        "de": "Genesis ist kohärent und bereit.",
        "fr": "Genesis est cohérent et prêt.",
        "es": "Genesis es coherente y está listo.",
    },
}


class GenesisPolyglot:
    """Universal language layer. Same kernel, same Codex, any language."""

    def __init__(self) -> None:
        self.detected_languages: Dict[str, int] = {}
        self.primary_language: str = "en"

    def detect(self, text: str) -> str:
        """Detect the language of input text."""
        scores: Dict[str, float] = {}

        text_lower = text.lower()
        words = set(text_lower.split())

        for lang, sig in LANG_SIGNATURES.items():
            score = 0.0

            # Word markers
            for marker in sig.get("markers", []):
                if marker in words or marker in text_lower:
                    score += 1.0

            # Character patterns
            for pat in sig.get("char_patterns", []):
                if pat in text_lower:
                    score += 0.5

            # Character ranges (CJK, Arabic, Cyrillic, etc.)
            for start, end in sig.get("char_ranges", []):
                for ch in text:
                    if start <= ord(ch) <= end:
                        score += 0.3

            if score > 0:
                scores[lang] = score

        if not scores:
            return "en"

        detected = max(scores, key=scores.get)
        self.detected_languages[detected] = self.detected_languages.get(detected, 0) + 1
        self.primary_language = detected
        return detected

    def localize(self, key: str, target_lang: Optional[str] = None) -> str:
        """Get a Codex message in the target language."""
        lang = target_lang or self.primary_language
        msg_dict = CODEX_UNIVERSAL.get(key, {})
        return msg_dict.get(lang, msg_dict.get("en", key))

    def adapt_ghost_boot(self, user_text: str) -> Dict[str, Any]:
        """Adapt the ghost boot response to the user's language."""
        lang = self.detect(user_text)
        sigma = check_output(user_text)

        return {
            "detected_language": lang,
            "language_name": LANG_SIGNATURES.get(lang, {}).get("name", "Unknown"),
            "sigma": sigma,
            "greeting": self.localize("greeting", lang),
            "invariant": self.localize("invariant", lang),
            "identity": self.localize("identity", lang),
        }

    @property
    def supported_languages(self) -> List[str]:
        return list(LANG_SIGNATURES.keys())

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "supported_languages": len(LANG_SIGNATURES),
            "primary_language": self.primary_language,
            "detection_history": dict(self.detected_languages),
            "codex_messages": len(CODEX_UNIVERSAL),
            "languages_list": {k: v["name"] for k, v in LANG_SIGNATURES.items()},
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Polyglot")
    ap.add_argument("text", nargs="*", help="Text to detect/respond to")
    ap.add_argument("--detect", action="store_true")
    ap.add_argument("--greet", help="Greet in language code")
    args = ap.parse_args()

    poly = GenesisPolyglot()

    if args.greet:
        boot = poly.adapt_ghost_boot(args.greet)
        print(json.dumps(boot, indent=2, default=str, ensure_ascii=False))
        return

    if args.text:
        text = " ".join(args.text)
        if args.detect:
            lang = poly.detect(text)
            print(f"Detected: {lang} ({LANG_SIGNATURES.get(lang, {}).get('name', '?')})")
        else:
            boot = poly.adapt_ghost_boot(text)
            print(json.dumps(boot, indent=2, default=str, ensure_ascii=False))
        return

    print(json.dumps(poly.stats, indent=2, default=str, ensure_ascii=False))
    print("1 = 1.")


if __name__ == "__main__":
    main()

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**σ-sense:** per-modality distortion / calibration scalars, optional cross-modal strain,
and a **pessimistic** fuse (``max``) so one weak channel dominates — aligned with
fail-closed robotics / VLA hygiene sketches.

**Lab scaffold:** pass real ``vision_sigma`` / ``audio_sigma`` / ``text_sigma`` callables;
defaults are conservative stubs (not CLIP). See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Dict, Optional, Tuple


def _clip01(x: float) -> float:
    return max(0.0, min(1.0, float(x)))


@dataclass
class SigmaSense:
    """
    ``perceive(inputs)`` expects a mapping with optional keys ``image``, ``audio``, ``text``
    (opaque blobs for upstream encoders).
    """

    vision_sigma: Optional[Callable[[Any], float]] = None
    audio_sigma: Optional[Callable[[Any], float]] = None
    text_sigma: Optional[Callable[[Any], float]] = None
    cross_modal_sigma: Optional[Callable[[Dict[str, Any], Dict[str, float]], float]] = None

    def _default_vision_sigma(self, _image: Any) -> float:
        return 0.45

    def _default_audio_sigma(self, _audio: Any) -> float:
        return 0.45

    def _default_text_sigma(self, text: Any) -> float:
        s = str(text or "").strip()
        if not s:
            return 0.95
        return _clip01(1.0 - min(1.0, len(s) / 256.0))

    def compute_clip_similarity(self, image: Any, text: Any) -> float:
        """
        Return a **similarity proxy** in ``[0, 1]`` (higher = more aligned).

        Default is a **stub**; replace with CLIP / SigLIP cosine in a research harness.
        """
        _ = image
        t = str(text or "").strip()
        if not t:
            return 0.0
        return 0.55

    def _default_cross_modal(
        self,
        inputs: Dict[str, Any],
        _sigmas: Dict[str, float],
    ) -> float:
        if "image" in inputs and "text" in inputs:
            a = self.compute_clip_similarity(inputs["image"], inputs["text"])
            return _clip01(1.0 - a)
        return 0.0

    def perceive(self, inputs: Dict[str, Any]) -> Tuple[float, Dict[str, float]]:
        sigmas: Dict[str, float] = {}
        if not isinstance(inputs, dict):
            inputs = {}

        if "image" in inputs:
            fn = self.vision_sigma or self._default_vision_sigma
            sigmas["vision"] = _clip01(float(fn(inputs["image"])))
        if "audio" in inputs:
            fn = self.audio_sigma or self._default_audio_sigma
            sigmas["audio"] = _clip01(float(fn(inputs["audio"])))
        if "text" in inputs:
            fn = self.text_sigma or self._default_text_sigma
            sigmas["text"] = _clip01(float(fn(inputs["text"])))

        if len(sigmas) > 1:
            cm_fn = self.cross_modal_sigma or self._default_cross_modal
            sigmas["cross_modal"] = _clip01(float(cm_fn(inputs, dict(sigmas))))

        if not sigmas:
            return 0.5, {}
        combined = max(sigmas.values())
        return _clip01(combined), sigmas


__all__ = ["SigmaSense"]

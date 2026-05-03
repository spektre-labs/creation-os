# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-vision — VLM **lab** scaffolding (text σ + grounding heuristics).

No pixel decoder bundled: pass attention maps / object lists from your VLM runtime.
See ``docs/CLAIM_DISCIPLINE.md`` — do not cite M3ID / GCoT percentages without harness JSON."""
from __future__ import annotations

import hashlib
import re
from typing import Any, Dict, List, Mapping, Optional, Sequence, Union

__all__ = ["SigmaVision"]

ImageRef = Union[str, bytes, None]


class SigmaVision:
    """Multimodal hooks: combine :class:`cos.sigma_gate.SigmaGate` text σ with grounding proxies."""

    modalities = ("text", "image", "audio", "video")

    @staticmethod
    def _image_fingerprint(image: ImageRef) -> str:
        if image is None:
            return "none"
        if isinstance(image, bytes):
            return hashlib.sha256(image[:4096]).hexdigest()[:16]
        return hashlib.sha256(str(image).encode("utf-8", errors="replace")).hexdigest()[:16]

    def score_vlm(
        self,
        image: ImageRef,
        prompt: str,
        response: str,
        gate: Any,
    ) -> Dict[str, Any]:
        sigma_t, ver_t = gate.score(str(prompt), str(response))
        # Grounding proxy: response should not be agnostic to image fingerprint hook.
        blob = f"{self._image_fingerprint(image)}|{str(response).lower()[:200]}"
        miss = 1.0 - min(1.0, len(set(blob)) / 80.0)
        sigma_g = float(max(0.0, min(1.0, miss * 0.4)))
        w_t, w_g = 0.55, 0.45
        sigma_c = w_t * float(sigma_t) + w_g * sigma_g
        return {
            "sigma_text": round(float(sigma_t), 6),
            "verdict_text": str(ver_t),
            "sigma_grounding": round(sigma_g, 6),
            "sigma_combined": round(float(sigma_c), 6),
            "note": "σ_grounding is a stub — wire bbox/segmentation overlap from grounding.py.",
        }

    @staticmethod
    def attention_to_image_ratio(attention_maps: Any) -> Dict[str, Any]:
        """``attention_maps``: optional ``{image: [...], text: [...]}`` mean mass or flat list [img_frac, ...]."""
        if attention_maps is None:
            return {"ratio": None, "risk_note": "no attention — cannot estimate language prior leakage"}
        if isinstance(attention_maps, Mapping):
            img = attention_maps.get("image") or attention_maps.get("vision")
            tx = attention_maps.get("text") or attention_maps.get("language")
            if isinstance(img, (list, tuple)) and isinstance(tx, (list, tuple)) and img and tx:
                mi, mt = sum(float(x) for x in img), sum(float(x) for x in tx)
                tot = mi + mt + 1e-9
                r = mi / tot
                return {
                    "ratio": round(r, 6),
                    "low_ratio_hallucination_risk": r < 0.25,
                }
        if isinstance(attention_maps, (list, tuple)) and attention_maps:
            r = float(sum(float(x) for x in attention_maps)) / max(len(attention_maps), 1)
            r = max(0.0, min(1.0, r))
            return {"ratio": round(r, 6), "low_ratio_hallucination_risk": r < 0.2}
        return {"ratio": 0.5, "low_ratio_hallucination_risk": False}

    @staticmethod
    def visual_grounding_check(response: str, image: ImageRef, objects: Sequence[str]) -> Dict[str, Any]:
        """Toy check: colour/object words vs ``objects`` joined string."""
        del image
        resp_l = str(response).lower()
        objs = [str(o).lower() for o in objects]
        objs_join = " ".join(objs)
        mentioned = [o for o in objs if o and o in resp_l]
        suspect: List[str] = []
        for m in re.findall(r"\b(red|blue|green|car|dog|cat|bus|person)\b", resp_l):
            if m not in objs_join:
                suspect.append(m)
        grounded_ok = len(suspect) == 0
        return {"supported_objects": mentioned, "suspect_tokens": suspect, "grounded_ok": grounded_ok}

    @staticmethod
    def sigma_per_token_visual(
        response_tokens: Sequence[str],
        image_attention: Sequence[float],
    ) -> List[Dict[str, Any]]:
        n = len(response_tokens)
        if not n:
            return []
        att = list(image_attention) if image_attention else [0.5] * n
        if len(att) < n:
            att = att + [att[-1]] * (n - len(att))
        rows: List[Dict[str, Any]] = []
        for i, tok in enumerate(response_tokens):
            a = float(att[min(i, len(att) - 1)])
            rows.append(
                {
                    "token": str(tok),
                    "image_attention": round(a, 6),
                    "looks_at_image": a >= 0.35,
                }
            )
        return rows

    def grounded_cot(
        self,
        prompt: str,
        image: ImageRef,
        gate: Any,
        *,
        regions: Optional[Sequence[Mapping[str, Any]]] = None,
    ) -> Dict[str, Any]:
        """GCoT-style **outline**: each step cites a region id + σ on the step text."""
        regs = list(regions) if regions else [{"id": "whole", "box": [0, 0, 1, 1]}]
        steps: List[Dict[str, Any]] = []
        fake_steps = [
            "Locate salient objects in the region of interest.",
            "Read any visible text or symbols tied to the question.",
            "Compose the final answer strictly from visible evidence.",
        ]
        for i, sentence in enumerate(fake_steps):
            reg = regs[min(i, len(regs) - 1)]
            sp = f"{prompt}\n[cot:{reg.get('id')}]: {sentence}"
            sigma, verdict = gate.score(sp, sentence)
            steps.append(
                {
                    "step": i + 1,
                    "text": sentence,
                    "region": dict(reg),
                    "sigma": round(float(sigma), 6),
                    "verdict": str(verdict),
                }
            )
        return {"prompt": str(prompt), "image_fp": self._image_fingerprint(image), "steps": steps}

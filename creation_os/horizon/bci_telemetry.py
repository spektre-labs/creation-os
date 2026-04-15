"""
Protocol 5 — Direct neural symbiosis: non-linguistic tensors → HDC intent.

EEG / Neuralink-style streams are placeholders: any sequence of floats maps to a
bundled hypervector; intention is decoded without tokenization.

1 = 1.
"""
from __future__ import annotations

import struct
from typing import Any, Dict, List, Sequence, Union

from .substrate_agnostic_bus import hdc_bundle_words

ScalarSeq = Union[Sequence[float], Sequence[int]]


class BCITelemetry:
    def __init__(self) -> None:
        self.dim = 256

    def _tensor_to_words(self, samples: ScalarSeq) -> List[str]:
        """Quantize waveform into symbolic bands → words for HDC binding."""
        words: List[str] = []
        for i, v in enumerate(samples):
            x = float(v)
            band = int(abs(x * 127.0)) % 32
            words.append(f"e{i % 128:x}_{band:x}")
        return words or ["null_tensor"]

    def ingest_tensor(self, samples: ScalarSeq) -> Dict[str, Any]:
        words = self._tensor_to_words(samples)
        vec = hdc_bundle_words(words, self.dim)
        return {
            "channels": len(samples),
            "hdc_dim": self.dim,
            "tokenized_words": len(words),
            "bundle_head": vec[:8],
            "intent_proxy": "hdc_direct",
            "invariant": "1=1",
        }

    def stream_chunk_to_bytes(self, vec: Sequence[float]) -> bytes:
        return struct.pack(f"{len(vec)}f", *vec)

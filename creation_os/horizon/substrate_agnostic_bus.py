"""
Protocol 1 — The Substrate Wall: hardware-agnostic HDC transport.

Unified memory, photonic link, or quantum I/O are substrates; the hypervector bundle
is the invariant. No re-encoding at boundaries — only dtype/layout negotiation.

1 = 1.
"""
from __future__ import annotations

import hashlib
import math
import struct
from dataclasses import dataclass, field
from typing import Any, Dict, List, Mapping, MutableMapping, Optional, Sequence, Tuple

try:
    import numpy as np

    _HAS_NP = True
except ImportError:
    np = None  # type: ignore[misc, assignment]
    _HAS_NP = False


def _word_seed(w: str) -> int:
    b = w.encode("utf-8", errors="ignore")
    h = 2166136261
    for c in b:
        h ^= c
        h = (h * 16777619) & 0xFFFFFFFF
    return h


def hdc_bundle_words(words: Sequence[str], dim: int) -> List[float]:
    """Bundled bipolar superposition (deterministic per word)."""
    if dim < 8 or dim % 2:
        raise ValueError("dim must be even and >= 8")
    acc = [0.0] * dim
    for w in words:
        s = _word_seed(w.lower())
        for i in range(dim):
            s = (s * 1103515245 + 12345) & 0x7FFFFFFF
            acc[i] += 1.0 if (s & 1) == 0 else -1.0
    n = math.sqrt(sum(x * x for x in acc)) or 1.0
    return [x / n for x in acc]


@dataclass
class HDCVessel:
    """
    Logical container for one HDC tensor + opaque substrate handle.
    The matrix is eternal; the vessel is where it is mounted.
    """

    name: str
    dim: int = 256
    logical_id: str = field(default_factory=lambda: "")
    payload: Optional[bytes] = None
    substrate_hint: str = "unified_memory"

    def __post_init__(self) -> None:
        if not self.logical_id:
            self.logical_id = hashlib.sha256(
                f"{self.name}:{self.dim}:{self.substrate_hint}".encode()
            ).hexdigest()[:32]

    def pack_bundle(self, words: Sequence[str]) -> None:
        vec = hdc_bundle_words(list(words), self.dim)
        if _HAS_NP:
            self.payload = np.asarray(vec, dtype=np.float32).tobytes()
        else:
            self.payload = struct.pack(f"{len(vec)}f", *vec)

    def export_portable(self) -> Dict[str, Any]:
        """Handoff blob for photonic / quantum / PCIe peer without semantic change."""
        return {
            "logical_id": self.logical_id,
            "dim": self.dim,
            "dtype": "float32",
            "layout": "row",
            "payload_b64": None if self.payload is None else __import__("base64").b64encode(self.payload).decode("ascii"),
            "invariant": "1=1",
            "substrate_hint": self.substrate_hint,
        }

    def import_portable(self, blob: Mapping[str, Any]) -> None:
        self.logical_id = str(blob.get("logical_id", self.logical_id))
        self.dim = int(blob.get("dim", self.dim))
        raw = blob.get("payload_b64")
        if raw:
            self.payload = __import__("base64").b64decode(str(raw))


class SubstrateAgnosticBus:
    """
    Registers vessels; routes the same HDC identity across substrates.
    """

    def __init__(self) -> None:
        self.vessels: MutableMapping[str, HDCVessel] = {}

    def register(self, vessel: HDCVessel) -> str:
        self.vessels[vessel.logical_id] = vessel
        return vessel.logical_id

    def attach_same_matrix(self, a_id: str, b_id: str) -> bool:
        """Assert two vessels carry the same logical_id / invariant (substrate bridge)."""
        va = self.vessels.get(a_id)
        vb = self.vessels.get(b_id)
        if not va or not vb:
            return False
        return va.logical_id == vb.logical_id and va.dim == vb.dim

    def canonical_snapshot(self) -> Dict[str, Any]:
        return {
            "vessel_count": len(self.vessels),
            "ids": list(self.vessels.keys())[:16],
            "invariant": "1=1",
        }

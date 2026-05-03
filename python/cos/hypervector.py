# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Bipolar BSC-style hypervectors (HDC lab): bind, bundle, permute, triple encoding, bit packing.

Requires **NumPy** (`pip install numpy`). Default dimension **10_000** matches the v65 stack narrative;
:meth:`HyperVector.memory_bytes` reports **int8** storage; :meth:`HyperVector.bitpacked_bytes` is **⌈D/8⌉**
(no implicit “tiny footprint” claims without packing).
"""
from __future__ import annotations

import hashlib
from typing import Any, Dict, List, Optional, Sequence, Tuple

__all__ = [
    "HyperVector",
    "HDCodebook",
    "SigmaHDC",
]

try:
    import numpy as np
except ImportError:  # pragma: no cover
    np = None  # type: ignore[misc, assignment]


def _require_numpy() -> Any:
    if np is None:  # pragma: no cover
        raise ImportError("cos.hypervector requires numpy: pip install numpy")
    return np


def _rng_from_seed(seed: int) -> Any:
    np = _require_numpy()
    return np.random.default_rng(int(seed) & 0xFFFFFFFF)


class HyperVector:
    """One bipolar vector in ``{-1,+1}^D`` (stored as ``int8``)."""

    __slots__ = ("_v", "_dim")

    def __init__(self, data: Any, *, dim: Optional[int] = None) -> None:
        np = _require_numpy()
        v = np.asarray(data, dtype=np.int8).reshape(-1)
        if dim is not None and v.shape[0] != int(dim):
            raise ValueError("HyperVector dim mismatch")
        if v.size == 0 or not bool(np.logical_or(v == 1, v == -1).all()):
            raise ValueError("HyperVector values must be in {-1,+1}")
        self._v = v
        self._dim: int = int(v.shape[0])

    @property
    def dim(self) -> int:
        return self._dim

    @property
    def array(self) -> Any:
        return self._v

    @classmethod
    def random(cls, dim: int, rng: Any) -> HyperVector:
        np = _require_numpy()
        bits = rng.integers(0, 2, size=dim, dtype=np.int8)
        v = np.where(bits == 0, np.int8(-1), np.int8(1))
        return cls(v, dim=dim)

    @classmethod
    def from_seed(cls, dim: int, label: str) -> HyperVector:
        """Reproducible vector from UTF-8 ``label`` (SHA-256–stirred bits)."""
        np = _require_numpy()
        h = hashlib.sha256(f"hv:{dim}:{label}".encode("utf-8")).digest()
        rng = _rng_from_seed(int.from_bytes(h[:8], "big"))
        return cls.random(dim, rng)

    def memory_bytes(self) -> int:
        """Bytes if stored as **dense int8** (honest accounting)."""
        return int(self._v.nbytes)

    def bitpacked_bytes(self) -> int:
        """Packed size **⌈D/8⌉** bits → bytes (one bit per dimension)."""
        return (self._dim + 7) // 8

    def to_bitpacked(self) -> bytes:
        """Pack +1→1, -1→0, MSB-first within each byte (first dimension in high bit of byte 0)."""
        out = bytearray(self.bitpacked_bytes())
        for i, bit in enumerate(((self._v[j] > 0) for j in range(self._dim))):
            if bit:
                out[i // 8] |= 1 << (7 - (i % 8))
        return bytes(out)

    @classmethod
    def from_bitpacked(cls, raw: bytes, *, dim: int) -> HyperVector:
        np = _require_numpy()
        need = (int(dim) + 7) // 8
        if len(raw) < need:
            raise ValueError("bitpacked payload too short for dim")
        buf = np.empty((int(dim),), dtype=np.int8)
        for i in range(int(dim)):
            b = raw[i // 8]
            bit = (b >> (7 - (i % 8))) & 1
            buf[i] = np.int8(1 if bit else -1)
        return cls(buf, dim=dim)

    @staticmethod
    def bind(a: HyperVector, b: HyperVector) -> HyperVector:
        np = _require_numpy()
        if a._dim != b._dim:
            raise ValueError("bind: dimension mismatch")
        return HyperVector(np.multiply(a._v, b._v, dtype=np.int8), dim=a._dim)

    @staticmethod
    def bundle(vectors: Sequence[HyperVector]) -> HyperVector:
        np = _require_numpy()
        if not vectors:
            raise ValueError("bundle: empty")
        dim = vectors[0]._dim
        acc = np.zeros((dim,), dtype=np.int32)
        for v in vectors:
            if v._dim != dim:
                raise ValueError("bundle: dimension mismatch")
            acc += v._v.astype(np.int32)
        out = np.sign(acc)
        out[out == 0] = 1
        return HyperVector(out.astype(np.int8), dim=dim)

    @staticmethod
    def permute(v: HyperVector, shifts: int = 1) -> HyperVector:
        np = _require_numpy()
        k = int(shifts) % v._dim
        return HyperVector(np.roll(v._v, k), dim=v._dim)

    @staticmethod
    def similarity(a: HyperVector, b: HyperVector) -> float:
        """Normalised dot product in ``[-1, 1]``."""
        np = _require_numpy()
        if a._dim != b._dim:
            raise ValueError("similarity: dimension mismatch")
        return float(np.dot(a._v.astype(np.float64), b._v.astype(np.float64)) / float(a._dim))


class HDCodebook:
    """Maps tokens to :class:`HyperVector` (content-addressable seeding)."""

    def __init__(self, dim: int = 10_000, *, seed: int = 0) -> None:
        self.dim = int(dim)
        self.seed = int(seed)
        self._cache: Dict[str, HyperVector] = {}

    def __getitem__(self, token: str) -> HyperVector:
        t = str(token)
        if t not in self._cache:
            self._cache[t] = HyperVector.from_seed(self.dim, f"{self.seed}:{t}")
        return self._cache[t]


class SigmaHDC:
    """Triple encoder + sequence σ from bipolar superposition noise (uses :class:`~cos.sigma_gate.SigmaGate` optionally)."""

    def __init__(self, dim: int = 10_000, *, seed: int = 0, gate: Any = None) -> None:
        self.dim = int(dim)
        self.book = HDCodebook(self.dim, seed=seed)
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()

    def encode_triple(self, subject: str, relation: str, obj: str) -> HyperVector:
        """``bind(S, bind(R, permute(O)))`` (atomic chaining)."""
        s = self.book[subject]
        r = self.book[relation]
        o = self.book[obj]
        return HyperVector.bind(s, HyperVector.bind(r, HyperVector.permute(o, 1)))

    def query_triple(self, trace: HyperVector, subject: str, relation: str) -> HyperVector:
        """Unbind ``subject`` and ``relation`` from ``trace`` → approx ``permute(object)``."""
        s = self.book[subject]
        r = self.book[relation]
        return HyperVector.bind(trace, HyperVector.bind(s, r))

    def sequence_sigma(self, tokens: Sequence[str], *, shift_per_step: int = 3) -> float:
        """Aggregate σ for a token sequence via gate on similarity deltas (lab surrogate)."""
        if len(tokens) < 2:
            return 0.0
        hvs = [self.book[t] for t in tokens]
        rolled: List[HyperVector] = []
        acc = hvs[0]
        rolled.append(acc)
        for i, hv in enumerate(hvs[1:], start=1):
            acc = HyperVector.bind(HyperVector.permute(acc, shift_per_step), hv)
            rolled.append(acc)
        sims = []
        for a, b in zip(rolled, rolled[1:], strict=False):
            sims.append(HyperVector.similarity(a, b))
        mean_s = sum(sims) / max(len(sims), 1)
        spread = sum(abs(x - mean_s) for x in sims) / max(len(sims), 1)
        prompt = "hv_sequence"
        response = f"mean_sim={mean_s:.4f} spread={spread:.4f}"
        sigma, _ = self.gate.score(prompt, response)
        return float(sigma)
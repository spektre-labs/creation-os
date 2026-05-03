# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-BitNet lab scaffold: packed ternary {-1,0,+1} matvec (add/sub/skip) and optional per-layer σ
using ``sigma_gate_core`` (not ``sigma_gate.h``).

This is a **reference** for Creation OS ternary inference; it does not load full BitNet GGUF
weights or replace ``bitnet.cpp``. Use ``cos bitnet`` for probes and synthetic timing.
"""
from __future__ import annotations

import hashlib
import struct
import time
from typing import Any, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update

COS_Q16 = 65536
GGUF_MAGIC_LE = 0x46554747
GGUF_MAGIC_ALT = 0x47475546


def pack_int8_ternary(w: Sequence[int], *, n: Optional[int] = None) -> bytes:
    """Pack ``n`` values in {-1,0,1} at 2 bits each (4 per byte), matching ``cos_infer_ternary_pack_int8``."""
    count = int(n if n is not None else len(w))
    out = bytearray((count + 3) // 4)
    for i in range(count):
        v = int(w[i])
        code = 1 if v > 0 else (2 if v < 0 else 0)
        out[i // 4] |= code << ((i % 4) * 2)
    return bytes(out)


def matvec_packed(packed: bytes, x: Sequence[int], rows: int, cols: int, scale_q16: int) -> List[int]:
    """int32 accumulator; ``output[r] = (acc * scale_q16) >> 16``."""
    y: List[int] = []
    sc = int(scale_q16) if scale_q16 > 0 else COS_Q16
    for r in range(rows):
        acc = 0
        for c in range(cols):
            lin = r * cols + c
            b = packed[lin // 4]
            code = (b >> ((lin % 4) * 2)) & 3
            xv = int(x[c])
            if code == 1:
                acc += xv
            elif code == 2:
                acc -= xv
        y.append(int((acc * sc) >> 16))
    return y


def sparsity_packed(packed: bytes, total_weights: int) -> float:
    zeros = 0
    for i in range(total_weights):
        b = packed[i // 4]
        code = (b >> ((i % 4) * 2)) & 3
        if code == 0:
            zeros += 1
    return zeros / float(total_weights) if total_weights > 0 else 0.0


def layer_sigma_q16_from_activations(act: Sequence[int]) -> float:
    mx = 0
    for v in act:
        a = abs(int(v))
        if a > mx:
            mx = a
    den = mx + 4000
    if den <= 0:
        return 0.0
    sig01 = min(0.9999, max(0.0, mx / float(den)))
    return float(sig01)


def stack_forward_sigma_gated(
    layers: Sequence[Tuple[bytes, int, int, int, int]],
    x0: Sequence[int],
    *,
    k_raw: float = 0.92,
    confident_exit_sigma: Optional[float] = None,
) -> Tuple[int, List[int], List[float]]:
    """
    Each layer: (packed, rows, cols, scale_q16, _pad).

    Returns ``(code, final_act, sigmas_q01)``.
    ``code == n_layers`` all OK; ``0 <= code < n_layers`` ABSTAIN after that layer;
    negative ``code`` means confident early exit at layer ``(-code) - 1``.
    """
    st = SigmaState()
    x = [int(v) for v in x0]
    sigs: List[float] = []
    out: List[int] = []
    for L, (pk, rows, cols, sc, _) in enumerate(layers):
        if len(x) != cols:
            raise ValueError("dimension mismatch")
        out = matvec_packed(pk, x, rows, cols, sc)
        sig01 = layer_sigma_q16_from_activations(out)
        sigs.append(sig01)
        sigma_update(st, float(sig01), float(k_raw))
        if confident_exit_sigma is not None and sig01 < float(confident_exit_sigma):
            return -L - 1, out, sigs
        if sigma_gate(st) == Verdict.ABSTAIN:
            return L, out, sigs
        x = out
    return len(layers), out, sigs


def probe_gguf_header(path: str) -> Dict[str, Any]:
    try:
        with open(path, "rb") as f:
            raw = f.read(4)
        if len(raw) < 4:
            return {"ok": False, "reason": "short file"}
        magic = struct.unpack("<I", raw)[0]
    except OSError as exc:
        return {"ok": False, "reason": str(exc)}
    ok = magic in (GGUF_MAGIC_LE, GGUF_MAGIC_ALT)
    return {"ok": bool(ok), "magic": f"0x{magic:08x}", "note": "header probe only — full tensor parse not in this module"}


def toy_layers_for_prompt(prompt: str, *, dim: int = 8, n_layers: int = 4) -> List[Tuple[bytes, int, int, int, int]]:
    """Deterministic tiny stack: square ``dim`` layers."""
    h = hashlib.sha256(prompt.encode("utf-8")).digest()
    layers: List[Tuple[bytes, int, int, int, int]] = []
    for L in range(n_layers):
        w: List[int] = []
        for i in range(dim * dim):
            b = h[(i + L * 17) % len(h)]
            w.append((b % 3) - 1)
        pk = pack_int8_ternary(w, n=dim * dim)
        sc = COS_Q16 // (4 + (h[L % len(h)] % 5))
        layers.append((pk, dim, dim, sc, 0))
    return layers


def bench_matvec_packed(*, dim: int = 128, repeats: int = 5) -> Dict[str, Any]:
    """Synthetic timing — not comparable to ``bitnet.cpp`` throughput claims."""
    n = max(8, int(dim))
    w = [((i * 13) % 3) - 1 for i in range(n * n)]
    pk = pack_int8_ternary(w)
    x = [(i * 11) % 101 - 50 for i in range(n)]
    t0 = time.perf_counter()
    for _ in range(repeats):
        matvec_packed(pk, x, n, n, COS_Q16)
    elapsed = max(1e-12, (time.perf_counter() - t0) / float(max(1, repeats)))
    return {
        "dim": n,
        "lab_ms_per_matvec": round(elapsed * 1000.0, 4),
        "note": "Python reference packed GEMV — not hardware BitNet kernels",
        "repeats": int(repeats),
    }


__all__ = [
    "bench_matvec_packed",
    "layer_sigma_q16_from_activations",
    "matvec_packed",
    "pack_int8_ternary",
    "probe_gguf_header",
    "sparsity_packed",
    "stack_forward_sigma_gated",
    "toy_layers_for_prompt",
]

# M4 AMX retro fingerprint — ctypes to libsk9_amx_retro.dylib (v7 .mm + AMX). 1 = 1
from __future__ import annotations

import ctypes
import hashlib
import os
import platform
import struct
from ctypes import c_int, c_uint64
from typing import Optional

_lib: Optional[ctypes.CDLL] = None


def _load() -> Optional[ctypes.CDLL]:
    global _lib
    if _lib is not None:
        return _lib
    if platform.system() != "Darwin" or platform.machine() not in ("arm64", "aarch64"):
        return None
    here = os.path.dirname(os.path.abspath(__file__))
    env = os.environ.get("CREATION_OS_AMX_RETRO_LIB", "").strip()
    candidates = [env] if env else []
    candidates.append(os.path.join(here, "libsk9_amx_retro.dylib"))
    for p in candidates:
        if p and os.path.isfile(p):
            try:
                lib = ctypes.CDLL(p)
                lib.sk9_amx_retro_fingerprint_u128.argtypes = [c_uint64, c_uint64]
                lib.sk9_amx_retro_fingerprint_u128.restype = ctypes.c_uint32
                lib.sk9_amx_retro_available.restype = c_int
                if hasattr(lib, "amx_retro_fingerprint"):
                    lib.amx_retro_fingerprint.argtypes = [ctypes.c_char_p]
                    lib.amx_retro_fingerprint.restype = ctypes.c_uint32
                if hasattr(lib, "sk9_amx_evaluate_sigma_soft"):
                    lib.sk9_amx_evaluate_sigma_soft.argtypes = [ctypes.c_char_p]
                    lib.sk9_amx_evaluate_sigma_soft.restype = c_int
                if hasattr(lib, "sk9_metal_snl_parallel"):
                    lib.sk9_metal_snl_parallel.argtypes = [
                        ctypes.POINTER(ctypes.c_uint32),
                        ctypes.POINTER(ctypes.c_uint32),
                        ctypes.c_size_t,
                        ctypes.POINTER(ctypes.c_uint32),
                        ctypes.POINTER(ctypes.c_uint32),
                        ctypes.POINTER(ctypes.c_uint32),
                    ]
                    lib.sk9_metal_snl_parallel.restype = c_int
                _lib = lib
                return _lib
            except OSError:
                _lib = None
    return None


def amx_retro_hw_available() -> bool:
    lib = _load()
    if lib is None:
        return False
    return int(lib.sk9_amx_retro_available()) != 0


def _scalar_fold(lo: int, hi: int) -> int:
    lo8 = lo.to_bytes(8, "little")
    hi8 = hi.to_bytes(8, "little")
    x = [c / 255.0 for c in lo8] + [c / 255.0 for c in hi8]
    z = [x[i] * 1.0 for i in range(16)]
    buf = b"".join(struct.pack("f", f) for f in z)
    h = 0x811C9DC5
    for b in buf:
        h ^= b
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def retro_fingerprint_u128(lo: int, hi: int) -> int:
    lib = _load()
    lo &= (1 << 64) - 1
    hi &= (1 << 64) - 1
    if lib is None:
        return _scalar_fold(lo, hi)
    return int(lib.sk9_amx_retro_fingerprint_u128(c_uint64(lo), c_uint64(hi)))


def retro_fingerprint_from_prompt_silicon(prompt: str) -> int:
    """CC_SHA256 + 4-row AMX fold inside dylib (libsk9_amx_retro.mm)."""
    lib = _load()
    if lib is None or not hasattr(lib, "amx_retro_fingerprint"):
        return 0
    buf = ctypes.create_string_buffer(prompt.encode("utf-8"))
    return int(lib.amx_retro_fingerprint(buf))


def retro_fingerprint_from_prompt(prompt: str) -> int:
    native = os.environ.get("CREATION_OS_AMX_RETRO_NATIVE", "").strip() in ("1", "true", "yes")
    if native:
        fp = retro_fingerprint_from_prompt_silicon(prompt)
        if fp != 0:
            return fp
    h = hashlib.sha256(prompt.strip().lower().encode("utf-8")).digest()[:16]
    lo = int.from_bytes(h[:8], "little")
    hi = int.from_bytes(h[8:], "little")
    return retro_fingerprint_u128(lo, hi)


def sigma_soft_from_amx(text: str) -> int:
    """Hardware σ proxy from sk9_amx_evaluate_sigma_soft (returns -1 if unavailable)."""
    lib = _load()
    if lib is None or not hasattr(lib, "sk9_amx_evaluate_sigma_soft"):
        return -1
    buf = ctypes.create_string_buffer(text.encode("utf-8"))
    return int(lib.sk9_amx_evaluate_sigma_soft(buf))


def metal_snl_parallel(
    lane_a: list[int],
    lane_b: list[int],
) -> Optional[tuple[list[int], list[int], list[int]]]:
    """GPU !, ^, & lanes; None if Metal path fails."""
    lib = _load()
    if lib is None or not hasattr(lib, "sk9_metal_snl_parallel"):
        return None
    n = len(lane_a)
    if n != len(lane_b) or n == 0:
        return None
    A = (ctypes.c_uint32 * n)(*lane_a)
    B = (ctypes.c_uint32 * n)(*lane_b)
    P = (ctypes.c_uint32 * n)()
    X = (ctypes.c_uint32 * n)()
    N = (ctypes.c_uint32 * n)()
    r = int(
        lib.sk9_metal_snl_parallel(
            ctypes.cast(A, ctypes.POINTER(ctypes.c_uint32)),
            ctypes.cast(B, ctypes.POINTER(ctypes.c_uint32)),
            ctypes.c_size_t(n),
            ctypes.cast(P, ctypes.POINTER(ctypes.c_uint32)),
            ctypes.cast(X, ctypes.POINTER(ctypes.c_uint32)),
            ctypes.cast(N, ctypes.POINTER(ctypes.c_uint32)),
        )
    )
    if r != 0:
        return None
    return (list(P), list(X), list(N))

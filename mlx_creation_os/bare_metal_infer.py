# v8/v9 ctypes — libsk9_bare_metal.dylib (AMX asm + mach_absolute_time). 1 = 1
from __future__ import annotations

import ctypes
import os
import platform
from ctypes import POINTER, byref, c_int, c_uint64
from typing import Any, Dict, Optional

_lib: Optional[ctypes.CDLL] = None


def _load() -> Optional[ctypes.CDLL]:
    global _lib
    if _lib is not None:
        return _lib
    if platform.system() != "Darwin" or platform.machine() not in ("arm64", "aarch64"):
        return None
    here = os.path.dirname(os.path.abspath(__file__))
    env = os.environ.get("CREATION_OS_BARE_METAL_LIB", "").strip()
    candidates = [env] if env else []
    candidates.append(os.path.join(here, "libsk9_bare_metal.dylib"))
    for p in candidates:
        if p and os.path.isfile(p):
            try:
                lib = ctypes.CDLL(p)
                lib.sk9_bare_amx_pulse_ns.restype = c_uint64
                lib.sk9_bare_amx_pulse_ns.argtypes = []
                lib.sk9_bare_amx_invariant_one.restype = c_int
                lib.sk9_bare_amx_invariant_one.argtypes = [c_int]
                lib.sk9_bare_amx_sigma_reject_dirty.restype = c_int
                lib.sk9_bare_amx_sigma_reject_dirty.argtypes = [c_uint64]
                if hasattr(lib, "sk9_amx_lock_enter"):
                    lib.sk9_amx_lock_enter.restype = None
                    lib.sk9_amx_lock_enter.argtypes = []
                if hasattr(lib, "sk9_amx_lock_exit"):
                    lib.sk9_amx_lock_exit.restype = None
                    lib.sk9_amx_lock_exit.argtypes = []
                if hasattr(lib, "sk9_amx_pulse_pipeline_z0"):
                    lib.sk9_amx_pulse_pipeline_z0.restype = None
                    lib.sk9_amx_pulse_pipeline_z0.argtypes = [
                        c_uint64,
                        c_uint64,
                        c_uint64,
                    ]
                if hasattr(lib, "sk9_hw_proof_amx_pulse"):
                    lib.sk9_hw_proof_amx_pulse.restype = c_int
                    lib.sk9_hw_proof_amx_pulse.argtypes = [
                        POINTER(c_uint64),
                        POINTER(c_uint64),
                        POINTER(c_int),
                    ]
                if hasattr(lib, "sk9_thread_rt_begin"):
                    lib.sk9_thread_rt_begin.restype = None
                    lib.sk9_thread_rt_begin.argtypes = []
                if hasattr(lib, "sk9_thread_rt_end"):
                    lib.sk9_thread_rt_end.restype = None
                    lib.sk9_thread_rt_end.argtypes = []
                _lib = lib
                return _lib
            except OSError:
                _lib = None
    return None


def bare_amx_pulse_ns() -> int:
    lib = _load()
    if lib is None:
        return 0
    return int(lib.sk9_bare_amx_pulse_ns())


def bare_amx_invariant_one() -> bool:
    lib = _load()
    if lib is None:
        return False
    return int(lib.sk9_bare_amx_invariant_one(0)) != 0


def bare_amx_reject_dirty(max_ns: int = 35_000) -> bool:
    """True if AMX pulse wall time exceeds max_ns (default 35µs)."""
    lib = _load()
    if lib is None:
        return False
    return int(lib.sk9_bare_amx_sigma_reject_dirty(c_uint64(int(max_ns)))) != 0


def bare_amx_ptr_row_flags(ptr_addr: int, row: int, flags: int) -> int:
    """Match AMX_PTR_ROW_FLAGS: 56-bit row/flags tag on a 256B-aligned host pointer."""
    return int(ptr_addr) + ((int(row) + int(flags) * 64) << 56)


def bare_hw_proof_amx_pulse() -> Optional[Dict[str, Any]]:
    """cntpct + exact double(1.0) + Q31 asm identity; rc bitmask 1=cycles 2=f64 4=int31."""
    lib = _load()
    if lib is None or not hasattr(lib, "sk9_hw_proof_amx_pulse"):
        return None
    cyc = c_uint64()
    ns = c_uint64()
    ex = c_int()
    rc = int(lib.sk9_hw_proof_amx_pulse(byref(cyc), byref(ns), byref(ex)))
    return {
        "cycles": int(cyc.value),
        "ns": int(ns.value),
        "exact_one": int(ex.value),
        "rc": rc,
        "dirty_cycles": bool(rc & 1),
        "bad_double_one": bool(rc & 2),
        "bad_int31": bool(rc & 4),
    }


def bare_amx_pulse_pipeline_z0(ldx_word: int, ldy_word: int, stz_word: int) -> bool:
    """v9: AMX SET → LDX(x19) LDY(x20) FMA32 vec Z0 → STZ(x21) → CLR. Returns False if symbol missing."""
    lib = _load()
    if lib is None or not hasattr(lib, "sk9_amx_pulse_pipeline_z0"):
        return False
    lib.sk9_amx_pulse_pipeline_z0(
        c_uint64(int(ldx_word)),
        c_uint64(int(ldy_word)),
        c_uint64(int(stz_word)),
    )
    return True

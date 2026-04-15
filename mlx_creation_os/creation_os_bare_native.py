# ctypes — libcreation_os.dylib (src/creation_os_core.mm). Single C entry; no Orion subprocess.
from __future__ import annotations

import ctypes
import os
import platform
from ctypes import POINTER, c_char_p, c_int, c_size_t, c_uint32, c_uint64, c_void_p
from typing import Any, Dict, Optional, Tuple

_LIB: Optional[ctypes.CDLL] = None


def _load() -> Optional[ctypes.CDLL]:
    global _LIB
    if _LIB is not None:
        return _LIB
    if platform.system() != "Darwin" or platform.machine() not in ("arm64", "aarch64"):
        return None
    here = os.path.dirname(os.path.abspath(__file__))
    p = os.environ.get("CREATION_OS_LIB", "").strip() or os.path.join(here, "libcreation_os.dylib")
    if not os.path.isfile(p):
        return None
    try:
        lib = ctypes.CDLL(p)
        lib.cos_bare_ctx_create.restype = c_void_p
        lib.cos_bare_ctx_create.argtypes = []
        lib.cos_bare_ctx_destroy.restype = None
        lib.cos_bare_ctx_destroy.argtypes = [c_void_p]
        lib.cos_bare_bbhash_attach.restype = c_int
        lib.cos_bare_bbhash_attach.argtypes = [c_void_p, c_char_p]
        lib.cos_bare_bbhash_detach.restype = None
        lib.cos_bare_bbhash_detach.argtypes = [c_void_p]
        lib.cos_bare_run_once.restype = c_int
        lib.cos_bare_run_once.argtypes = [
            c_void_p,
            c_char_p,
            c_size_t,
            c_int,
            POINTER(c_int),
            POINTER(c_uint32),
            POINTER(c_int),
            POINTER(c_uint64),
            c_char_p,
            c_size_t,
        ]
        _LIB = lib
        return _LIB
    except OSError:
        _LIB = None
    return None


def bare_run_once(
    prompt: str,
    *,
    bbhash_mmap: str = "",
    fire_gpu_ane: bool = True,
) -> Optional[Tuple[str, Dict[str, Any]]]:
    lib = _load()
    if lib is None:
        return None
    ctx = lib.cos_bare_ctx_create()
    if not ctx:
        return None
    try:
        if bbhash_mmap:
            lib.cos_bare_bbhash_attach(ctx, bbhash_mmap.encode("utf-8"))
        b = prompt.encode("utf-8")
        route = c_int(0)
        bbv = c_uint32(0)
        sigma = c_int(0)
        key = c_uint64(0)
        buf = ctypes.create_string_buffer(4096)
        rc = int(
            lib.cos_bare_run_once(
                ctx,
                b,
                len(b),
                1 if fire_gpu_ane else 0,
                ctypes.byref(route),
                ctypes.byref(bbv),
                ctypes.byref(sigma),
                ctypes.byref(key),
                buf,
                len(buf),
            )
        )
        if rc != 0:
            return None
        text = buf.value.decode("utf-8", errors="replace")
        rec: Dict[str, Any] = {
            "orion_kernel": False,
            "creation_os_bare_native": True,
            "bare_route": int(route.value),
            "bare_bbhash_val": int(bbv.value),
            "bare_sigma": int(sigma.value),
            "bare_key": int(key.value),
        }
        return text, rec
    finally:
        lib.cos_bare_ctx_destroy(ctx)


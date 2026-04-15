# Creation OS ↔ Orion ANE kernel orchestration (Tier0 BBHash → Tier1 σ CPU → ANE IOSurface path).
# ANE↔GPU fence: Orion uses IOSurface; maderix/ANE documents _ANESharedSignalEvent for GPU pairing.
from __future__ import annotations

import os
import subprocess
import threading
import time
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

_REPO = Path(__file__).resolve().parents[1]
_ORION_ROOT = Path(os.environ.get("CREATION_OS_ORION_ROOT",
                   str(_REPO / "external" / "Orion")))
_ORION_BIN = Path(os.environ.get("CREATION_OS_ORION_BIN", str(_ORION_ROOT / "orion")))


def orion_root() -> Path:
    return _ORION_ROOT


def orion_cli() -> Path:
    return _ORION_BIN


def tier0_bbhash_before_ane(
    sk9: Any,
    prompt: str,
    mmap_path: str,
    tier0_shadow_skip: bool,
) -> Optional[int]:
    """Return BBHash fact id if Tier0 hits; call before any ANE dispatch."""
    if not mmap_path or sk9 is None or not getattr(sk9, "ok", False):
        return None
    if os.environ.get("CREATION_OS_BBHASH_TIER0_SHORT", "1").strip() not in ("1", "true", "yes"):
        return None
    try:
        from creation_os_core import query_hash_key
        from sk9_bindings import SK9_ROUTE_BBHASH
    except ImportError:
        return None
    qk = query_hash_key(prompt)
    route, val = sk9.epistemic_route(qk)
    if (not tier0_shadow_skip) and route == SK9_ROUTE_BBHASH and val is not None:
        return int(val)
    return None


def tier1_sigma_cpu_kernel(prompt: str, sk9: Any) -> Dict[str, Any]:
    """POPCNT / CLZ on query key bytes (CPU σ probe, no framework)."""
    try:
        from creation_os_core import query_hash_key
    except ImportError:
        return {"popcount": 0, "clz": 0, "sigma_sk9": -1}
    qk = query_hash_key(prompt)
    raw = qk.encode("utf-8") if isinstance(qk, str) else bytes(qk)
    pc = sum(bin(b).count("1") for b in raw)
    clz = 0
    if raw:
        first = raw[0]
        for i in range(8):
            if (first >> (7 - i)) & 1:
                break
            clz += 1
    sig = -1
    if sk9 is not None and getattr(sk9, "ok", False):
        try:
            sig = int(sk9.evaluate_sigma(prompt))
        except Exception:
            sig = -1
    return {"popcount": pc, "clz": clz, "sigma_sk9": sig, "qk_len": len(raw)}


def _living_weights_metal_warm_thread() -> None:
    try:
        import mlx.core as mx

        _ = mx.array([1.0], dtype=mx.float32)
        mx.eval(_)
    except Exception:
        pass


def kick_living_weights_metal_parallel() -> None:
    """Fire-and-forget GPU graph compile touch while ANE path prepares (same process)."""
    if os.environ.get("CREATION_OS_ORION_METAL_WARM", "1").strip() not in ("1", "true", "yes"):
        return
    t = threading.Thread(target=_living_weights_metal_warm_thread, daemon=True)
    t.start()


def try_orion_ane_route(
    prompt: str,
    mlx_model_path: str,
    max_tokens: int,
    temp: float,
    top_p: float,
    sk9: Any,
    mmap_path: str,
    tier0_shadow_skip: bool,
    sigma_margin: int,
    _facts_json: str,
) -> Optional[Tuple[str, Dict[str, Any]]]:
    """
    Tier1 σ → optional Metal warm → Orion `orion infer --ane`.
    Caller must run BBHash Tier0 before this (creation_os_generate_native order).
    CREATION_OS_ORION_KERNEL=1 and built third_party/Orion/orion required.
    """
    if os.environ.get("CREATION_OS_ORION_KERNEL", "").strip() not in ("1", "true", "yes"):
        return None
    if os.environ.get("CREATION_OS_BARE_NATIVE_FIRST", "1").strip() in ("1", "true", "yes"):
        try:
            import creation_os_bare_native as _bare

            mmap_p = mmap_path or os.environ.get("SPEKTRE_BBHASH_MMAP", "").strip()
            bh = _bare.bare_run_once(
                prompt,
                bbhash_mmap=mmap_p,
                fire_gpu_ane=os.environ.get("CREATION_OS_BARE_FIRE_GPU", "1").strip()
                in ("1", "true", "yes"),
            )
            if bh is not None:
                text_b, rec_b = bh
                rec_b.setdefault("sigma", int(rec_b.get("bare_sigma", 0)))
                rec_b["coherent"] = True
                rec_b["prompt"] = prompt
                rec_b["latency_total_us"] = 1
                rec_b["max_tokens"] = int(max_tokens)
                return text_b, rec_b
        except Exception:
            pass
    if not _ORION_BIN.is_file():
        return None

    t0 = time.perf_counter_ns()
    tier1 = tier1_sigma_cpu_kernel(prompt, sk9)
    kick_living_weights_metal_parallel()

    weights = os.environ.get("CREATION_OS_ORION_WEIGHTS", "").strip() or "model/blobs/gpt2_124m"
    vocab = os.environ.get("CREATION_OS_ORION_VOCAB", "").strip() or "tokenizer/data/vocab.json"
    merges = os.environ.get("CREATION_OS_ORION_MERGES", "").strip() or "tokenizer/data/merges.txt"
    mode = os.environ.get("CREATION_OS_ORION_ANE_MODE", "full").strip().lower()
    extra: list[str] = []
    if mode == "prefill":
        extra.append("--ane-prefill")
    else:
        extra.append("--ane")

    argv = [
        str(_ORION_BIN),
        "infer",
        "--prompt",
        prompt,
        "--max_tokens",
        str(int(max_tokens)),
        "--temperature",
        str(float(temp)),
        "--top_p",
        str(float(top_p)),
        "--weights",
        weights,
        "--vocab",
        vocab,
        "--merges",
        merges,
        *extra,
    ]
    try:
        proc = subprocess.run(
            argv,
            cwd=str(_ORION_ROOT),
            capture_output=True,
            text=True,
            timeout=int(os.environ.get("CREATION_OS_ORION_TIMEOUT_SEC", "600")),
        )
    except (subprocess.TimeoutExpired, OSError):
        return None
    lat_us = max(1, (time.perf_counter_ns() - t0) // 1000)
    if proc.returncode != 0:
        return None
    text_out = (proc.stdout or "").strip()
    if not text_out:
        return None

    try:
        from creation_os_core import (
            apply_abi_of_silence_to_receipt,
            check_output,
            execution_tier_for_attestation,
            is_coherent,
            is_manifest_stutter,
            mixture_sigma_tier_for_receipt,
            receipt_dict,
        )
    except ImportError:
        return None

    sigma_final = check_output(text_out)
    if sk9 is not None and getattr(sk9, "ok", False):
        try:
            s = int(sk9.evaluate_sigma(text_out))
            if s >= 0:
                sigma_final = max(0, s)
        except Exception:
            pass
    coherent = is_coherent(text_out)
    receipt = receipt_dict(sigma_final, coherent, prompt, 0, False, False)
    receipt["sk9_native"] = bool(sk9 is not None and getattr(sk9, "ok", False))
    receipt["orion_kernel"] = True
    receipt["orion_ane_mode"] = mode
    receipt["orion_mlx_model_shadow"] = mlx_model_path
    receipt["tier1_popcount"] = tier1.get("popcount", 0)
    receipt["tier1_clz"] = tier1.get("clz", 0)
    receipt["tier1_sigma_sk9_prompt"] = tier1.get("sigma_sk9", -1)
    receipt["latency_total_us"] = int(lat_us)
    receipt["latency_tier0_us"] = 0
    receipt["latency_tier1_kernel_us"] = int(lat_us)
    receipt["latency_model_load_us"] = 0
    receipt["latency_tier234_transform_us"] = int(lat_us)
    receipt["tokens_generated"] = 0
    receipt["max_tokens"] = int(max_tokens)
    receipt["mixture_sigma_tier"] = mixture_sigma_tier_for_receipt(
        False,
        bool(sk9 is not None and getattr(sk9, "ok", False)),
        1,
        True,
    )
    receipt["tier_used"] = execution_tier_for_attestation(receipt, max_tokens=max_tokens)
    receipt["dynamic_sigma_margin"] = int(sigma_margin)
    receipt["shadow_prompt_hit"] = bool(tier0_shadow_skip)
    apply_abi_of_silence_to_receipt(receipt, sk9=sk9)
    receipt["manifest_stutter"] = bool(is_manifest_stutter(text_out))
    receipt["stutter_halted"] = False
    receipt["sigma_after"] = int(sigma_final)
    return text_out, receipt

#!/usr/bin/env python3
"""
PHASE 3: ANE HDC Resonance — O(1) fact-space dot product via Apple Neural Engine.

Architecture:
  1. Encode facts.json → INT8 matrix (N_facts × HDC_dim) via hv_encode.
  2. At query time: encode query → HV, matmul against fact matrix → top-K indices.
  3. PRIMARY path: Maderix ANE bridge (direct _ANEInMemoryModel, bypasses CoreML).
     Uses 1×1 conv trick: matmul expressed as convolution = 3× throughput on ANE.
  4. Secondary: CoreML ANE (_ANEClient via coremltools).
  5. Fallback: MLX mx.matmul on Metal GPU (zero-copy, unified memory).

Usage:
  resonator = ANEHDCResonator("facts.json", hdc_dim=10000)
  top_facts = resonator.query("what is the capital of finland", top_k=3)

Environment:
  CREATION_OS_ANE_HDC=1          enable ANE path (default: MLX Metal fallback)
  CREATION_OS_HDC_DIM=10000      hypervector dimension
  CREATION_OS_ANE_HDC_TOP_K=5    default top-K results
  CREATION_OS_ANE_BRIDGE_LIB=    path to libane_bridge.dylib

1 = 1.
"""
from __future__ import annotations

import ctypes
import hashlib
import json
import os
import struct
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

try:
    import mlx.core as mx
except ImportError:
    mx = None  # type: ignore

try:
    import numpy as np
except ImportError:
    np = None  # type: ignore

HDC_DIM = int(os.environ.get("CREATION_OS_HDC_DIM", "10000"))
HDC_W = (HDC_DIM + 63) // 64

# ── ANE Bridge (Maderix direct access) ──────────────────────────────────
_ane_lib: Optional[ctypes.CDLL] = None
_ANE_BRIDGE_LOADED = False


def _load_ane_bridge() -> Optional[ctypes.CDLL]:
    """Load libane_bridge.dylib for direct ANE access."""
    global _ane_lib, _ANE_BRIDGE_LOADED
    if _ANE_BRIDGE_LOADED:
        return _ane_lib

    _ANE_BRIDGE_LOADED = True
    candidates = [
        os.environ.get("CREATION_OS_ANE_BRIDGE_LIB", ""),
        os.path.join(os.path.dirname(__file__), "..", "external", "ANE", "bridge", "libane_bridge.dylib"),
        os.path.join(os.path.dirname(__file__), "libane_bridge.dylib"),
    ]
    for path in candidates:
        if path and os.path.isfile(path):
            try:
                lib = ctypes.CDLL(path)
                lib.ane_bridge_init.restype = ctypes.c_int
                lib.ane_bridge_compile.restype = ctypes.c_void_p
                lib.ane_bridge_compile.argtypes = [
                    ctypes.c_char_p, ctypes.c_size_t,
                    ctypes.c_void_p, ctypes.c_size_t,
                    ctypes.c_int, ctypes.POINTER(ctypes.c_size_t),
                    ctypes.c_int, ctypes.POINTER(ctypes.c_size_t),
                ]
                lib.ane_bridge_eval.restype = ctypes.c_bool
                lib.ane_bridge_eval.argtypes = [ctypes.c_void_p]
                lib.ane_bridge_write_input.restype = None
                lib.ane_bridge_write_input.argtypes = [
                    ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t,
                ]
                lib.ane_bridge_read_output.restype = None
                lib.ane_bridge_read_output.argtypes = [
                    ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t,
                ]
                lib.ane_bridge_free.restype = None
                lib.ane_bridge_free.argtypes = [ctypes.c_void_p]
                lib.ane_bridge_build_weight_blob_int8.restype = ctypes.c_void_p
                lib.ane_bridge_build_weight_blob_int8.argtypes = [
                    ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t),
                ]

                rc = lib.ane_bridge_init()
                if rc == 0:
                    _ane_lib = lib
                    print(f"[ANE-HDC] Maderix ANE bridge loaded: {path}", file=sys.stderr)
                    return lib
                else:
                    print(f"[ANE-HDC] ANE bridge init failed (rc={rc})", file=sys.stderr)
            except Exception as e:
                print(f"[ANE-HDC] ANE bridge load failed: {e}", file=sys.stderr)
    return None


def _generate_hdc_mil(n_facts: int, dim: int) -> str:
    """Generate MIL program for HDC matmul as 1×1 convolution (ANE native primitive).

    The trick: reshape query (1, dim) → (1, dim, 1, 1) and fact matrix (n_facts, dim)
    as conv weights (n_facts, dim, 1, 1). Conv = matmul but 3× faster on ANE because
    convolution is the ANE's native op.
    """
    return f"""program(1.3)
[buildInfo = dict<string, string>({{{{"coremlc-component-MIL", "3510.2.1"}}, {{"coremlc-version", "3505.4.1"}}, {{"coremltools-component-milinternal", ""}}, {{"coremltools-version", "9.0"}}}})]
{{
    func main<ios18>(tensor<fp16, [1, {dim}, 1, 1]> query) {{
        string pad_type = const()[name = string("pad_type"), val = string("valid")];
        tensor<int32, [2]> strides = const()[name = string("strides"), val = tensor<int32, [2]>([1, 1])];
        tensor<int32, [4]> pad = const()[name = string("pad"), val = tensor<int32, [4]>([0, 0, 0, 0])];
        tensor<int32, [2]> dilations = const()[name = string("dilations"), val = tensor<int32, [2]>([1, 1])];
        int32 groups = const()[name = string("groups"), val = int32(1)];
        tensor<fp16, [{n_facts}, {dim}, 1, 1]> W = constexpr_affine_dequantize()[axis = int32(0), name = string("W"), quantized_data = tensor<int8, [{n_facts}, {dim}, 1, 1]>(BLOBFILE(path = string("@model_path/weights/weight.bin"), offset = uint64(64))), scale = fp16(0x1p0), zero_point = int8(0)];
        tensor<fp16, [1, {n_facts}, 1, 1]> scores = conv(dilations = dilations, groups = groups, pad = pad, pad_type = pad_type, strides = strides, weight = W, x = query)[name = string("scores")];
    }} -> (scores);
}}\n"""


def _hv_encode_py(text: str, dim: int = HDC_DIM) -> List[int]:
    """Pure-Python hv_encode matching core/hdc.c: FNV-1a seeded bit flips → int8 {-1,+1}."""
    bits = [0] * dim
    state = 0xCBF29CE484222325
    mask64 = (1 << 64) - 1
    for i, ch in enumerate(text):
        state = ((state ^ ord(ch)) * 0x100000001B3) & mask64
        pos = int((state ^ i) % dim)
        bits[pos] ^= 1
        pos2 = int((((state >> 17) ^ (i * 1315423911)) & mask64) % dim)
        bits[pos2] ^= 1
    return [1 if b else -1 for b in bits]


def _hv_encode_batch_int8(texts: List[str], dim: int = HDC_DIM) -> Any:
    """Encode all texts → (N, dim) int8 matrix. Returns np.ndarray if numpy available, else list."""
    rows = [_hv_encode_py(t, dim) for t in texts]
    if np is not None:
        return np.array(rows, dtype=np.int8)
    return rows


class ANEHDCResonator:
    """HDC fact resonator: encodes facts into a matrix, queries via matmul.

    Priority: Maderix ANE bridge (direct) > CoreML ANE > MLX Metal > NumPy CPU.
    """

    def __init__(self, facts_json_path: str, hdc_dim: int = HDC_DIM) -> None:
        self.dim = hdc_dim
        self.facts: List[Dict[str, str]] = []
        self.keys: List[str] = []
        self.values: List[str] = []
        self._fact_matrix_np: Optional[Any] = None
        self._fact_matrix_mx: Optional[Any] = None
        self._fact_matrix_int8: Optional[Any] = None
        self._ane_kernel: Optional[int] = None  # ANE bridge kernel handle (as c_void_p int)
        self._ane_coreml_model: Optional[Any] = None
        self._load_facts(facts_json_path)
        self._encode_facts()
        self._try_ane_bridge()
        if self._ane_kernel is None:
            self._try_ane_coreml()

    def _load_facts(self, path: str) -> None:
        if not os.path.isfile(path):
            return
        with open(path) as f:
            data = json.load(f)
        if isinstance(data, list):
            for item in data:
                if isinstance(item, dict) and "q" in item and "a" in item:
                    self.keys.append(str(item["q"]).strip().lower())
                    self.values.append(str(item["a"]).strip())
                elif isinstance(item, (list, tuple)) and len(item) >= 2:
                    self.keys.append(str(item[0]).strip().lower())
                    self.values.append(str(item[1]).strip())
        elif isinstance(data, dict):
            for k, v in data.items():
                self.keys.append(str(k).strip().lower())
                self.values.append(str(v).strip())

    def _encode_facts(self) -> None:
        if not self.keys:
            return
        t0 = time.perf_counter()
        mat = _hv_encode_batch_int8(self.keys, self.dim)
        if np is not None:
            self._fact_matrix_np = mat
            self._fact_matrix_int8 = mat  # keep int8 for ANE bridge
            if mx is not None:
                self._fact_matrix_mx = mx.array(mat.astype(np.float16))
        dt = time.perf_counter() - t0
        print(f"[ANE-HDC] Encoded {len(self.keys)} facts ({self.dim}D) in {dt*1000:.1f} ms",
              file=sys.stderr)

    def _try_ane_bridge(self) -> None:
        """Primary ANE path: Maderix direct bridge with 1×1 conv trick."""
        if self._fact_matrix_int8 is None or len(self.keys) == 0:
            return
        lib = _load_ane_bridge()
        if lib is None:
            return

        n_facts = len(self.keys)
        dim = self.dim
        t0 = time.perf_counter()

        mil_text = _generate_hdc_mil(n_facts, dim)
        mil_bytes = mil_text.encode("utf-8")

        # Build int8 weight blob matching ANE benchmark format:
        # [64-byte outer header] [64-byte chunk header] [int8 data]
        # MIL BLOBFILE offset=64 points to chunk header; ANE reads data at chunk+64.
        int8_flat = self._fact_matrix_int8.flatten().astype(np.int8)
        wsize = len(int8_flat)
        chunk_size = 64 + wsize
        total = 64 + chunk_size
        weight_blob = bytearray(total)
        # Outer header
        weight_blob[0] = 0x01; weight_blob[4] = 0x02
        # Chunk header at offset 64
        weight_blob[64] = 0xEF; weight_blob[65] = 0xBE
        weight_blob[66] = 0xAD; weight_blob[67] = 0xDE
        weight_blob[68] = 0x01; weight_blob[74] = 0x08
        # Int8 data at offset 128
        weight_blob[128:128 + wsize] = int8_flat.tobytes()

        wb_arr = (ctypes.c_uint8 * total).from_buffer_copy(weight_blob)
        input_sz = (ctypes.c_size_t * 1)(dim * 2)  # fp16 input: dim * 2 bytes
        output_sz = (ctypes.c_size_t * 1)(n_facts * 2)  # fp16 output: n_facts * 2 bytes

        kernel = lib.ane_bridge_compile(
            mil_bytes, len(mil_bytes),
            ctypes.cast(wb_arr, ctypes.c_void_p), total,
            1, input_sz,
            1, output_sz,
        )

        if kernel:
            self._ane_kernel = kernel
            dt = time.perf_counter() - t0
            print(f"[ANE-HDC] Maderix ANE kernel compiled: {n_facts} facts × {dim}D "
                  f"(1×1 conv, {dt*1000:.0f} ms)", file=sys.stderr)
        else:
            print("[ANE-HDC] ANE bridge compile failed, falling back", file=sys.stderr)

    def _try_ane_coreml(self) -> None:
        """Secondary ANE path: CoreML compilation."""
        if os.environ.get("CREATION_OS_ANE_HDC", "").strip() not in ("1", "true", "yes"):
            return
        if self._fact_matrix_np is None or len(self.keys) == 0:
            return
        try:
            import coremltools as ct
            from coremltools.converters.mil import Builder as mb
            from coremltools.converters.mil.mil import types as mtypes

            n_facts = len(self.keys)
            dim = self.dim

            @mb.program(
                input_specs=[mb.TensorSpec(shape=(1, dim), dtype=mtypes.fp16)],
            )
            def hdc_matmul(query_hv):
                W = mb.const(val=self._fact_matrix_np.astype(np.float16).T)
                scores = mb.matmul(x=query_hv, y=W)
                return scores

            mlmodel = ct.convert(
                hdc_matmul,
                compute_units=ct.ComputeUnit.ALL,
                minimum_deployment_target=ct.target.iOS17,
            )
            self._ane_coreml_model = mlmodel
            print(f"[ANE-HDC] CoreML ANE model compiled: {n_facts} facts × {dim}D",
                  file=sys.stderr)
        except Exception as e:
            print(f"[ANE-HDC] CoreML compile failed (Metal fallback): {e}", file=sys.stderr)

    def query(self, text: str, top_k: int = 5) -> List[Tuple[str, str, float]]:
        """Query the fact space. Returns list of (question, answer, score) tuples.

        Priority: ANE bridge > CoreML ANE > MLX Metal > NumPy CPU.
        """
        if not self.keys:
            return []
        qhv = _hv_encode_py(text.strip().lower(), self.dim)

        if self._ane_kernel is not None:
            return self._query_ane_bridge(qhv, top_k)

        if self._ane_coreml_model is not None:
            return self._query_coreml(qhv, top_k)

        if mx is not None and self._fact_matrix_mx is not None:
            return self._query_mlx(qhv, top_k)

        if np is not None and self._fact_matrix_np is not None:
            return self._query_numpy(qhv, top_k)

        return []

    def _query_ane_bridge(self, qhv: List[int], top_k: int) -> List[Tuple[str, str, float]]:
        """Maderix ANE bridge: direct _ANEInMemoryModel eval via IOSurface."""
        lib = _ane_lib
        if lib is None or np is None:
            return self._query_mlx(qhv, top_k) if mx is not None else []

        q_fp16 = np.array(qhv, dtype=np.float16)
        input_bytes = q_fp16.tobytes()
        lib.ane_bridge_write_input(
            self._ane_kernel, 0,
            input_bytes, len(input_bytes),
        )

        ok = lib.ane_bridge_eval(self._ane_kernel)
        if not ok:
            return self._query_mlx(qhv, top_k) if mx is not None else []

        n_facts = len(self.keys)
        out_buf = (ctypes.c_uint8 * (n_facts * 2))()
        lib.ane_bridge_read_output(
            self._ane_kernel, 0,
            ctypes.cast(out_buf, ctypes.c_void_p), n_facts * 2,
        )
        scores = np.frombuffer(bytes(out_buf), dtype=np.float16).astype(np.float32)
        idx = np.argsort(scores)[::-1][:top_k]
        return [(self.keys[i], self.values[i], float(scores[i])) for i in idx]

    def _query_coreml(self, qhv: List[int], top_k: int) -> List[Tuple[str, str, float]]:
        """CoreML ANE path."""
        if np is None:
            return self._query_mlx(qhv, top_k) if mx is not None else []
        q = np.array([qhv], dtype=np.float16)
        try:
            out = self._ane_coreml_model.predict({"query_hv": q})
            scores_key = list(out.keys())[0]
            scores = np.array(out[scores_key]).flatten()
        except Exception:
            return self._query_mlx(qhv, top_k) if mx is not None else []
        idx = np.argsort(scores)[::-1][:top_k]
        return [(self.keys[i], self.values[i], float(scores[i])) for i in idx]

    def _query_mlx(self, qhv: List[int], top_k: int) -> List[Tuple[str, str, float]]:
        """Metal fallback: mx.matmul on unified memory."""
        q = mx.array([qhv], dtype=mx.float16)
        scores = mx.matmul(q, self._fact_matrix_mx.T)
        mx.eval(scores)
        s_np = np.array(scores[0])
        idx = np.argsort(s_np)[::-1][:top_k]
        return [(self.keys[i], self.values[i], float(s_np[i])) for i in idx]

    def _query_numpy(self, qhv: List[int], top_k: int) -> List[Tuple[str, str, float]]:
        """CPU fallback."""
        q = np.array(qhv, dtype=np.float32)
        scores = self._fact_matrix_np.astype(np.float32) @ q
        idx = np.argsort(scores)[::-1][:top_k]
        return [(self.keys[i], self.values[i], float(scores[i])) for i in idx]

    def __del__(self) -> None:
        if self._ane_kernel is not None and _ane_lib is not None:
            try:
                _ane_lib.ane_bridge_free(self._ane_kernel)
            except Exception:
                pass
            self._ane_kernel = None

    @property
    def n_facts(self) -> int:
        return len(self.keys)

    @property
    def backend(self) -> str:
        if self._ane_kernel is not None:
            return "ane_bridge"
        if self._ane_coreml_model is not None:
            return "coreml_ane"
        if self._fact_matrix_mx is not None:
            return "mlx_metal"
        if self._fact_matrix_np is not None:
            return "numpy_cpu"
        return "none"


_global_resonator: Optional[ANEHDCResonator] = None


def get_resonator(facts_json: Optional[str] = None) -> Optional[ANEHDCResonator]:
    """Singleton accessor."""
    global _global_resonator
    if _global_resonator is not None:
        return _global_resonator
    path = facts_json or os.environ.get("SPEKTRE_BBHASH_FACTS_JSON", "").strip()
    if not path or not os.path.isfile(path):
        return None
    _global_resonator = ANEHDCResonator(path)
    return _global_resonator


def hdc_resonance_for_prompt(prompt: str, top_k: int = 3,
                             facts_json: Optional[str] = None) -> List[Tuple[str, str, float]]:
    """Top-level convenience: get top-K resonant facts for a prompt."""
    r = get_resonator(facts_json)
    if r is None:
        return []
    return r.query(prompt, top_k=top_k)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="ANE HDC Resonance test")
    parser.add_argument("facts_json", help="Path to facts.json")
    parser.add_argument("query", help="Query string")
    parser.add_argument("--top-k", type=int, default=5)
    args = parser.parse_args()

    res = ANEHDCResonator(args.facts_json)
    print(f"Loaded {res.n_facts} facts  |  backend: {res.backend}")
    t0 = time.perf_counter()
    results = res.query(args.query, args.top_k)
    dt = time.perf_counter() - t0
    print(f"Query took {dt*1000:.2f} ms")
    for i, (q, a, s) in enumerate(results):
        print(f"  [{i+1}] score={s:.1f}  Q: {q}  A: {a}")

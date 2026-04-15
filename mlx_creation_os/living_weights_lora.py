#!/usr/bin/env python3
"""
PHASE 4: Living Weights LoRA — σ-driven online adapter refinement.

When Creation OS generates a response:
  - Tokens with σ → 0 (attested, coherent) reinforce the adapter.
  - Tokens with σ > threshold (firmware leak, hallucination) create negative training signal.
  - The LoRA adapter is patched incrementally using MLX's built-in fine-tuning.

Architecture:
  - Accumulate (prompt, response, sigma_trace) pairs in a rolling buffer.
  - When buffer reaches BATCH_SIZE, run a single LoRA fine-tune step.
  - Adapter weights are saved periodically to --adapter-path.

Environment:
  CREATION_OS_LIVING_LORA=1                 enable online LoRA updates
  CREATION_OS_LIVING_LORA_PATH=<dir>        adapter save directory
  CREATION_OS_LIVING_LORA_BATCH=8           accumulation batch size
  CREATION_OS_LIVING_LORA_LR=1e-5           learning rate
  CREATION_OS_LIVING_LORA_SIGMA_THR=0.1     sigma threshold for negative signal

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    import mlx.core as mx
    import mlx.nn as nn
    import mlx.optimizers as optim
except ImportError:
    mx = None  # type: ignore
    nn = None  # type: ignore
    optim = None  # type: ignore

ENABLED = os.environ.get("CREATION_OS_LIVING_LORA", "").strip() in ("1", "true", "yes")
ADAPTER_DIR = os.environ.get("CREATION_OS_LIVING_LORA_PATH", "artifacts/living_lora")
BATCH_SIZE = int(os.environ.get("CREATION_OS_LIVING_LORA_BATCH", "8"))
LR = float(os.environ.get("CREATION_OS_LIVING_LORA_LR", "1e-5"))
SIGMA_THR = float(os.environ.get("CREATION_OS_LIVING_LORA_SIGMA_THR", "0.1"))


class LivingLoRABuffer:
    """Rolling buffer of (tokens, sigma_per_token) pairs for online LoRA."""

    def __init__(self, batch_size: int = BATCH_SIZE) -> None:
        self.batch_size = batch_size
        self.buffer: List[Dict[str, Any]] = []
        self.total_updates = 0
        self.total_samples = 0

    def push(self, prompt_tokens: List[int], response_tokens: List[int],
             sigma_trace: List[Dict[str, Any]]) -> bool:
        """Add a sample. Returns True if batch is full and ready for update."""
        self.buffer.append({
            "prompt_tokens": prompt_tokens,
            "response_tokens": response_tokens,
            "sigma_trace": sigma_trace,
        })
        self.total_samples += 1
        return len(self.buffer) >= self.batch_size

    def flush(self) -> List[Dict[str, Any]]:
        batch = self.buffer[:]
        self.buffer.clear()
        self.total_updates += 1
        return batch

    @property
    def pending(self) -> int:
        return len(self.buffer)


def _compute_token_weights(sigma_trace: List[Dict[str, Any]], n_tokens: int) -> List[float]:
    """Per-token weight: σ→0 tokens get weight 1.0; σ>threshold get weight -0.5 (negative signal)."""
    weights = [1.0] * n_tokens
    for entry in sigma_trace:
        idx = entry.get("token_index", -1)
        sigma_after = float(entry.get("sigma_after", 0))
        if 0 <= idx < n_tokens:
            if sigma_after > SIGMA_THR:
                weights[idx] = -0.5
            else:
                weights[idx] = 1.0
    return weights


def living_lora_step(
    model: Any,
    tokenizer: Any,
    batch: List[Dict[str, Any]],
    adapter_dir: str = ADAPTER_DIR,
    lr: float = LR,
) -> Dict[str, Any]:
    """Run a single LoRA fine-tune step on the accumulated batch.

    Returns dict with loss, n_tokens, elapsed_ms.
    """
    if mx is None or nn is None or optim is None:
        return {"error": "mlx not available"}

    t0 = time.perf_counter()
    Path(adapter_dir).mkdir(parents=True, exist_ok=True)

    total_loss = 0.0
    total_tokens = 0

    optimizer = optim.Adam(learning_rate=lr)

    for sample in batch:
        prompt_toks = sample["prompt_tokens"]
        resp_toks = sample["response_tokens"]
        sigma_trace = sample.get("sigma_trace", [])
        all_toks = prompt_toks + resp_toks
        if len(all_toks) < 2:
            continue

        weights = [0.0] * len(prompt_toks) + _compute_token_weights(sigma_trace, len(resp_toks))

        input_ids = mx.array([all_toks[:-1]])
        target_ids = mx.array([all_toks[1:]])
        w = mx.array([weights[1:]])

        def loss_fn(m: Any) -> mx.array:
            logits = m(input_ids)
            logits_flat = logits.reshape(-1, logits.shape[-1])
            targets_flat = target_ids.reshape(-1)
            w_flat = w.reshape(-1)
            ce = mx.take(
                -mx.log(mx.softmax(logits_flat, axis=-1) + 1e-10),
                targets_flat[:, None],
                axis=-1,
            ).squeeze(-1)
            return mx.mean(ce * w_flat)

        loss, grads = nn.value_and_grad(model, loss_fn)(model)
        optimizer.update(model, grads)
        mx.eval(model.parameters())

        total_loss += float(loss)
        total_tokens += len(resp_toks)

    elapsed_ms = (time.perf_counter() - t0) * 1000

    try:
        adapter_weights_path = os.path.join(adapter_dir, "adapters.safetensors")
        lora_params = {k: v for k, v in model.parameters().items() if "lora" in k.lower()}
        if lora_params:
            mx.savez(adapter_weights_path, **lora_params)
            # Orion hot-swap: export updated LoRA as BLOBFILE for ANE reload
            _export_lora_for_orion(lora_params, adapter_dir)
    except Exception:
        pass

    return {
        "loss": total_loss / max(len(batch), 1),
        "n_tokens": total_tokens,
        "n_samples": len(batch),
        "elapsed_ms": elapsed_ms,
    }


def _export_lora_for_orion(lora_params: dict, adapter_dir: str) -> None:
    """Export MLX LoRA weights as Orion BLOBFILE format for ANE hot-swap.

    Orion uses 128-byte header + fp16 data per weight file.
    Directory layout: adapter_dir/layerN/lora_{target}_{A|B}.bin

    When Orion's `orion_program_reload_weights` is called, it reads these
    files to update ANE without recompilation (delta compilation).
    """
    if mx is None:
        return
    import struct as _struct

    orion_dir = os.path.join(adapter_dir, "orion_ane")
    os.makedirs(orion_dir, exist_ok=True)

    for param_name, param_val in lora_params.items():
        parts = param_name.split(".")
        layer_idx = None
        target = None
        ab = None
        for i, p in enumerate(parts):
            if p.startswith("layers") and i + 1 < len(parts) and parts[i + 1].isdigit():
                layer_idx = int(parts[i + 1])
            if p in ("q_proj", "k_proj", "v_proj", "o_proj"):
                target = p[0]
            if p.startswith("lora_"):
                ab = p.split("_")[-1].upper()

        if layer_idx is None or target is None or ab is None:
            continue

        layer_dir = os.path.join(orion_dir, f"layer{layer_idx}")
        os.makedirs(layer_dir, exist_ok=True)

        mx.eval(param_val)
        arr = param_val.astype(mx.float16)
        mx.eval(arr)
        flat = arr.reshape(-1).tolist()
        rows, cols = param_val.shape

        # BLOBFILE: 128-byte header + fp16 data
        wsize = rows * cols * 2
        total = 128 + wsize
        buf = bytearray(total)
        buf[0] = 0x01; buf[4] = 0x02
        buf[64] = 0xEF; buf[65] = 0xBE; buf[66] = 0xAD; buf[67] = 0xDE
        buf[68] = 0x01
        _struct.pack_into("<I", buf, 72, wsize)
        _struct.pack_into("<I", buf, 80, 128)

        offset = 128
        for v in flat:
            _struct.pack_into("<e", buf, offset, float(v))
            offset += 2

        out_path = os.path.join(layer_dir, f"lora_{target}_{ab}.bin")
        with open(out_path, "wb") as f:
            f.write(buf)


_global_buffer: Optional[LivingLoRABuffer] = None


def get_buffer() -> LivingLoRABuffer:
    global _global_buffer
    if _global_buffer is None:
        _global_buffer = LivingLoRABuffer()
    return _global_buffer


def living_lora_on_generation(
    model: Any,
    tokenizer: Any,
    prompt_tokens: List[int],
    response_tokens: List[int],
    sigma_trace: List[Dict[str, Any]],
) -> Optional[Dict[str, Any]]:
    """Call after each generation. Accumulates and auto-fires LoRA step when batch full."""
    if not ENABLED:
        return None
    buf = get_buffer()
    ready = buf.push(prompt_tokens, response_tokens, sigma_trace)
    if ready:
        batch = buf.flush()
        result = living_lora_step(model, tokenizer, batch)
        print(f"[LIVING-LORA] Step {buf.total_updates}: loss={result.get('loss', '?'):.4f} "
              f"tokens={result.get('n_tokens', 0)} in {result.get('elapsed_ms', 0):.0f}ms",
              file=sys.stderr)
        return result
    return None

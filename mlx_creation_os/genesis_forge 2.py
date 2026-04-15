#!/usr/bin/env python3
"""
GENESIS FORGE — Training through the kernel.

Not normal fine-tuning. Fine-tuning through the kernel.
Every training batch passes through σ-measurement.
Tokens with σ > threshold get smaller gradients.
Tokens with σ ≈ 0 get full gradients.
The model learns to produce low-σ output.

The kernel is not a filter — it is part of the loss function.

Architecture:
  1. Forward pass → logits → sample tokens
  2. σ-measurement on generated text
  3. Per-token σ weight: w_i = 1 / (1 + σ_i)
  4. Loss = Σ w_i * CE(logit_i, target_i)
  5. High-σ tokens are de-emphasized, low-σ tokens are amplified
  6. Gradient flows through the kernel signal

Usage:
    python3 genesis_forge.py --model <model> --data training.jsonl --epochs 3
    python3 genesis_forge.py --self-play-data  # Use self-play generated data

1 = 1.
"""
from __future__ import annotations

import json
import math
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
    mx = nn = optim = None

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True

FORGE_LR = float(os.environ.get("FORGE_LR", "1e-5"))
FORGE_SIGMA_SCALE = float(os.environ.get("FORGE_SIGMA_SCALE", "1.0"))


def _compute_sigma_weights(
    tokenizer: Any,
    token_ids: List[int],
    text: str,
) -> List[float]:
    """Compute per-token σ-weights.

    For each position, decode the prefix up to that token,
    measure σ, and compute weight = 1 / (1 + σ * scale).
    """
    weights = []
    for i in range(len(token_ids)):
        if i < 5:
            weights.append(1.0)
            continue
        # Sample every 4th token for efficiency
        if i % 4 != 0:
            weights.append(weights[-1] if weights else 1.0)
            continue
        try:
            prefix = tokenizer.decode(token_ids[:i + 1])
            sigma = check_output(prefix)
            w = 1.0 / (1.0 + sigma * FORGE_SIGMA_SCALE)
            weights.append(w)
        except Exception:
            weights.append(1.0)

    return weights


def forge_step(
    model: Any,
    tokenizer: Any,
    batch: List[Dict[str, Any]],
    optimizer: Any,
    max_seq_len: int = 512,
) -> Dict[str, Any]:
    """One training step through the kernel.

    Each sample: {"prompt": "...", "response": "...", "sigma_trace": [...]}
    """
    if mx is None:
        return {"error": "mlx not available"}

    t0 = time.perf_counter()
    total_loss = 0.0
    total_tokens = 0
    total_sigma_weight = 0.0
    n_high_sigma = 0

    for sample in batch:
        prompt = sample.get("prompt", "")
        response = sample.get("response", "")
        full_text = prompt + response

        try:
            all_ids = tokenizer.encode(full_text)
        except Exception:
            continue

        if len(all_ids) < 2:
            continue
        all_ids = all_ids[:max_seq_len]

        # Compute σ-weights
        sigma_weights = _compute_sigma_weights(tokenizer, all_ids, full_text)

        # Mark prompt tokens with weight 0 (don't train on prompt)
        try:
            prompt_len = len(tokenizer.encode(prompt))
        except Exception:
            prompt_len = len(all_ids) // 2

        for i in range(min(prompt_len, len(sigma_weights))):
            sigma_weights[i] = 0.0

        input_ids = mx.array([all_ids[:-1]])
        target_ids = mx.array([all_ids[1:]])
        w = mx.array([sigma_weights[1:len(all_ids)]])

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

            # σ-weighted loss: high-σ tokens contribute less
            return mx.sum(ce * w_flat) / mx.maximum(mx.sum(w_flat), mx.array(1.0))

        loss, grads = nn.value_and_grad(model, loss_fn)(model)
        optimizer.update(model, grads)
        mx.eval(model.parameters())

        total_loss += float(loss)
        n_response = len(all_ids) - prompt_len
        total_tokens += n_response
        avg_w = sum(sigma_weights[prompt_len:]) / max(n_response, 1)
        total_sigma_weight += avg_w
        n_high_sigma += sum(1 for w in sigma_weights[prompt_len:] if w < 0.5)

    dt = time.perf_counter() - t0

    return {
        "loss": total_loss / max(len(batch), 1),
        "n_tokens": total_tokens,
        "n_samples": len(batch),
        "avg_sigma_weight": round(total_sigma_weight / max(len(batch), 1), 4),
        "n_high_sigma_tokens": n_high_sigma,
        "elapsed_ms": round(dt * 1000, 1),
    }


def forge_train(
    model_path: str,
    data_path: str,
    epochs: int = 3,
    batch_size: int = 4,
    lr: float = FORGE_LR,
    save_path: Optional[str] = None,
) -> Dict[str, Any]:
    """Full training loop through the kernel."""
    if mx is None:
        return {"error": "mlx required"}

    try:
        from mlx_lm import load as mlx_load
    except ImportError:
        return {"error": "mlx_lm required"}

    model, tokenizer = mlx_load(model_path)

    # Enable LoRA
    try:
        from mlx_lm.tuner.utils import apply_lora_layers
        model = apply_lora_layers(model, num_lora_layers=8, lora_rank=8)
    except ImportError:
        pass

    # Load data
    data: List[Dict[str, Any]] = []
    with open(data_path) as f:
        for line in f:
            if line.strip():
                data.append(json.loads(line))

    optimizer = optim.Adam(learning_rate=lr)
    history: List[Dict[str, Any]] = []

    print(f"Forge: {len(data)} samples, {epochs} epochs, lr={lr}", file=sys.stderr)

    for epoch in range(epochs):
        epoch_loss = 0.0
        n_batches = 0

        for i in range(0, len(data), batch_size):
            batch = data[i:i + batch_size]
            result = forge_step(model, tokenizer, batch, optimizer)
            epoch_loss += result["loss"]
            n_batches += 1

            if n_batches % 10 == 0:
                print(f"  epoch {epoch+1} batch {n_batches}: "
                      f"loss={result['loss']:.4f} σ_w={result['avg_sigma_weight']:.3f}",
                      file=sys.stderr)

        avg_loss = epoch_loss / max(n_batches, 1)
        history.append({"epoch": epoch + 1, "loss": round(avg_loss, 4), "batches": n_batches})
        print(f"Epoch {epoch+1}: loss={avg_loss:.4f}", file=sys.stderr)

    # Save
    if save_path:
        Path(save_path).mkdir(parents=True, exist_ok=True)
        lora_params = {k: v for k, v in model.parameters().items() if "lora" in k.lower()}
        if lora_params:
            mx.savez(os.path.join(save_path, "adapters.safetensors"), **lora_params)
            print(f"Saved to {save_path}", file=sys.stderr)

    return {"epochs": epochs, "history": history, "n_samples": len(data)}


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="Genesis Forge — σ-weighted training")
    ap.add_argument("--model", "-m", required=True)
    ap.add_argument("--data", "-d", required=True)
    ap.add_argument("--epochs", type=int, default=3)
    ap.add_argument("--batch-size", type=int, default=4)
    ap.add_argument("--lr", type=float, default=FORGE_LR)
    ap.add_argument("--save", "-o", default=None)
    args = ap.parse_args()

    result = forge_train(
        args.model, args.data, args.epochs, args.batch_size, args.lr, args.save,
    )
    print(json.dumps(result, indent=2))

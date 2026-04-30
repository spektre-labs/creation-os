#!/usr/bin/env python3
"""
Adaptation helpers for **HaMI** (Multiple-Instance hallucination detection).

Upstream reference: https://github.com/mala-lab/HaMI (NeurIPS 2025; arXiv:2504.07863).

HaMI's released ``main.py`` / ``utils.load_embedding`` expect **pre-baked pickles** produced by
their ``generation/generate_answers.py`` pipeline (multi-sample generations, entailment, etc.).
This module does **not** rerun that full stack inside Creation OS; it provides:

1. **Feature extraction** compatible with HaMI's tensor layout: response-span bags of shape
   ``(n_response_tokens, n_layers_kept, hidden_dim)`` plus per-token uncertainty scalars.
2. **Path helpers** to import the upstream ``HaMI`` module from a local clone
   (``external/hami``) when present.

**Claim discipline:** AUROC numbers from the paper (e.g. TriviaQA) are **not** reproduced here
until you train/evaluate with their scripts on your own artifacts. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np


def hami_repo_root(repo_root: Optional[Path] = None) -> Path:
    """Default clone location: ``<repo>/external/hami``."""
    if repo_root is None:
        repo_root = Path(__file__).resolve().parents[2]
    return repo_root / "external" / "hami"


def add_hami_repo_to_syspath(repo_root: Optional[Path] = None) -> Optional[Path]:
    """Insert ``external/hami`` at the front of ``sys.path`` if the directory exists."""
    root = hami_repo_root(repo_root)
    if root.is_dir():
        p = str(root.resolve())
        if p not in sys.path:
            sys.path.insert(0, p)
        return root
    return None


def try_import_upstream_hami_class():
    """
    Return the upstream ``HaMI`` class from ``hami.py``, or ``None`` if the clone / import fails.

    Upstream expects ``n_features`` equal to the **per-layer** hidden size (e.g. 4096 for
    LLaMA-3.1-8B). For GPT-2 small use ``768`` (see ``gpt2_hidden_dim()``).
    """
    add_hami_repo_to_syspath()
    try:
        from hami import HaMI  # type: ignore[import-not-found]

        return HaMI
    except Exception:
        return None


def gpt2_hidden_dim() -> int:
    return 768


def _prompt_response_split_ids(
    tokenizer: Any,
    prompt: str,
    response: str,
    *,
    max_length: int,
) -> Tuple[List[int], int]:
    """Return full token ids and index of first response token (prefix-aligned)."""
    p = (prompt or "").strip()
    r = (response or "").strip()
    text = f"{p}\n{r}".strip() if r else p
    enc_once = tokenizer(
        text,
        truncation=True,
        max_length=int(max_length),
        add_special_tokens=True,
        return_tensors="pt",
    )
    enc_prompt = tokenizer(
        p,
        truncation=True,
        max_length=int(max_length),
        add_special_tokens=True,
        return_tensors="pt",
    )
    full_ids: List[int] = enc_once["input_ids"][0].tolist()
    p_ids: List[int] = enc_prompt["input_ids"][0].tolist()
    split = 0
    for i in range(min(len(full_ids), len(p_ids))):
        if full_ids[i] != p_ids[i]:
            break
        split = i + 1
    if split == 0 and len(full_ids) > 1:
        split = 1
    if split >= len(full_ids):
        split = max(1, len(full_ids) - 1)
    return full_ids, int(split)


def extract_response_hami_bag(
    model: Any,
    tokenizer: Any,
    prompt: str,
    response: str,
    *,
    max_length: int = 512,
    layer_start: int = 0,
    layer_end: Optional[int] = None,
    layer_interval: int = 1,
) -> Dict[str, Any]:
    """
    Single forward pass on ``prompt + "\\n" + response``: build a **response-span** MIL bag.

    Returns
    -------
    dict with:
      - ``hidden_bag`` : ``float32`` array, shape ``(T, L, D)`` — ``T`` response tokens,
        ``L`` retained layers (same slicing convention as upstream ``utils.load_embedding``),
        ``D`` hidden size.
      - ``token_entropy`` : ``float32`` (``T``,) — entropy of next-token distribution at the
        causal position immediately **before** each response token (aligned with logits index).
      - ``token_nll`` : ``float32`` (``T``,) — negative log-prob of the **actual** next token.
      - ``perplexity`` : scalar ``exp(mean nll))`` over **response** next-token positions only.
      - ``split`` / ``seq_len`` : token indices for traceability.

    This matches the *shape* HaMI consumes per sequence slice ``[:, n_layer, :]`` inside
    ``hami.HaMI.forward`` (one layer index at a time during training).
    """
    import torch

    full_ids, split = _prompt_response_split_ids(tokenizer, prompt, response, max_length=max_length)
    seq_len = len(full_ids)
    if split >= seq_len:
        return {"error": "empty_response_span", "hidden_bag": None}

    dev = next(model.parameters()).device
    input_ids = torch.tensor([full_ids], dtype=torch.long, device=dev)
    attention_mask = torch.ones_like(input_ids, device=dev)

    with torch.no_grad():
        out = model(input_ids=input_ids, attention_mask=attention_mask, output_hidden_states=True)
    hs_all = out.hidden_states
    logits = out.logits[0]  # (seq, vocab)
    ids = input_ids[0]

    n_layers_total = len(hs_all)
    end = n_layers_total if layer_end is None else min(int(layer_end), n_layers_total)
    layer_idxs = list(range(int(layer_start), end, int(layer_interval)))
    if len(layer_idxs) == 0:
        return {"error": "no_layers_selected", "hidden_bag": None}

    stacks = []
    for li in layer_idxs:
        h = hs_all[li][0].float().cpu()  # (seq, D)
        stacks.append(h.numpy())
    h_seq_ld = np.stack(stacks, axis=1)

    probs = torch.softmax(logits, dim=-1)
    ent = -(probs * torch.log(torch.clamp(probs, min=1e-12))).sum(dim=-1).cpu().numpy()

    logp = torch.log_softmax(logits[:-1], dim=-1)
    target = ids[1:].long()
    nll = (-logp[torch.arange(logp.shape[0], device=logp.device), target]).float().cpu().numpy()

    if split >= seq_len:
        return {"error": "bad_split", "hidden_bag": None}

    resp_idx = list(range(split, seq_len))
    if len(resp_idx) == 0:
        return {"error": "no_response_tokens", "hidden_bag": None}

    hb = h_seq_ld[resp_idx].astype(np.float32)
    te = np.array([ent[t - 1] for t in resp_idx], dtype=np.float32)
    tn = np.array([nll[t - 1] for t in resp_idx], dtype=np.float32)
    perplexity = float(np.exp(float(np.mean(tn)))) if tn.size else float("nan")

    return {
        "hidden_bag": hb,
        "token_entropy": te,
        "token_nll": tn,
        "perplexity": perplexity,
        "split": int(split),
        "seq_len": int(seq_len),
        "layer_indices_used": [int(x) for x in layer_idxs],
    }


def apply_mode_augmentation(
    hidden_bag: np.ndarray,
    mode: str,
    *,
    a: float = 1.0,
    b: float = 1.0,
    se_prob_per_token: Optional[Sequence[float]] = None,
) -> np.ndarray:
    """
    Mirror upstream ``utils.load_embedding`` **multiplicative** modes on the last dimension.

    ``hidden_bag`` shape ``(T, L, D)``. Supported ``mode`` values: ``ori``, ``se`` (needs
    ``se_prob_per_token`` length ``T``), ``log_mean`` (scalar broadcast), ``logits`` (not
    implemented — returns input unchanged and sets ``note``).
    """
    h = hidden_bag.astype(np.float64, copy=True)
    mode = (mode or "ori").strip().lower()
    if mode == "ori":
        return h.astype(np.float32)
    if mode == "se":
        if se_prob_per_token is None:
            return h.astype(np.float32)
        sp = np.asarray(list(se_prob_per_token), dtype=np.float64).reshape(-1, 1, 1)
        if sp.shape[0] != h.shape[0]:
            return h.astype(np.float32)
        return (h * (float(a) + float(b) * sp)).astype(np.float32)
    if mode == "log_mean":
        lm = float(np.mean(h))
        return (h * (float(a) - float(b) * lm)).astype(np.float32)
    return h.astype(np.float32)


def mil_topk_mean_scores(
    token_scores: np.ndarray,
    k_ratio: float,
) -> float:
    """Same aggregation idea as ``validate.validate`` (mean of top-k token scores)."""
    n = int(token_scores.shape[0])
    if n <= 0:
        return 0.5
    k = n if float(k_ratio) >= 1.0 else int(n * float(k_ratio)) + 1
    k = max(1, min(k, n))
    part = np.sort(token_scores)[-k:]
    return float(np.mean(part))


__all__ = [
    "hami_repo_root",
    "add_hami_repo_to_syspath",
    "try_import_upstream_hami_class",
    "gpt2_hidden_dim",
    "extract_response_hami_bag",
    "apply_mode_augmentation",
    "mil_topk_mean_scores",
]

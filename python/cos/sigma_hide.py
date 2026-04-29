# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-side **HIDE** channel: training-free dependence score between **input** and **output**
hidden representations.

**Official path** (when ``keybert`` + ``torch`` are available): runs the same **greedy**
``model.generate(..., output_hidden_states=True)`` layout as the public reference repo
`C-anwoy/HIDE <https://github.com/C-anwoy/HIDE>`__ and scores with their **unbiased HSIC +
KeyBERT token selection** pipeline (vendored verbatim core in ``hide_metric_upstream.py``,
ported from ``func/metric.py`` / ``func/kernels.py`` — see file header there).

**Lab path:** single forward on ``prompt + "\\n" + response`` with ``output_hidden_states=True``.
At a **middle transformer block** (or ``layer=`` if set), take the last ``n`` prompt-token
hidden vectors and the first ``n`` continuation-token vectors (``n = min(n_prompt, n_cont)``
and ``n >= 2``). Build RBF Gram matrices ``K``, ``L`` on those two sets and evaluate the
same **zero-diagonal unbiased HSIC** estimator as ``hide_metric_upstream.unbiased_HSIC``.
**Risk score:** ``sigma_hide = 1 - clip(HSIC / scale, 0, 1)`` with ``scale`` a Frobenius
norm of the zero-diagonal kernels so high dependence maps to low σ. Short contexts raise
``RuntimeError`` (no silent neutral scores).

**Claim discipline:** This is a **runtime harness** aligned with the published *code*
layout — not a guarantee of paper-reported AUROC without a local calibration run on your
model and splits (``docs/CLAIM_DISCIPLINE.md``).

**Convention:** higher ``sigma_hide`` ≈ more hallucination-oriented risk (low dependence
→ higher score via a fixed squashing curve).
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Optional, Tuple

import numpy as np


def _pairwise_sq_dist(x: np.ndarray) -> np.ndarray:
    return np.sum((x[:, None, :] - x[None, :, :]) ** 2, axis=-1)


def _rbf_kernel(x: np.ndarray, sigma: Optional[float] = None) -> Tuple[np.ndarray, float]:
    x = np.asarray(x, dtype=np.float64)
    sq = _pairwise_sq_dist(x)
    off = sq[np.triu_indices(sq.shape[0], k=1)]
    if sigma is None:
        med = float(np.median(off)) if off.size else 1.0
        sigma = float(np.sqrt(max(med, 1e-12) / 2.0))
    sigma = max(float(sigma), 1e-6)
    return np.exp(-sq / (2.0 * sigma**2)), sigma


def _hsic_unbiased_zero_diag(K: np.ndarray, L: np.ndarray) -> float:
    """
    Unbiased HSIC with **zeroed diagonals**, matching ``hide_metric_upstream.unbiased_HSIC``
    (same algebra, NumPy) so the **lab** token path stays comparable to the official KeyBERT
    pipeline in this tree.
    """
    n = int(K.shape[0])
    if n < 2 or L.shape[0] != n or L.shape[1] != n:
        return float("nan")
    Kt = np.array(K, dtype=np.float64, copy=True)
    Lt = np.array(L, dtype=np.float64, copy=True)
    np.fill_diagonal(Kt, 0.0)
    np.fill_diagonal(Lt, 0.0)
    tr = float(np.trace(Kt @ Lt))
    sum_k = float(np.sum(Kt))
    sum_l = float(np.sum(Lt))
    triple = float(np.sum(Kt @ Lt))
    hsic = tr + (sum_k * sum_l) / (float(n) * float(n)) - (2.0 / float(n)) * triple
    return float(hsic / (float(n) * float(n)))


def _upstream_deps_ok() -> bool:
    try:
        from .hide_metric_upstream import upstream_metric_dependencies_available

        return bool(upstream_metric_dependencies_available())
    except Exception:
        return False


def _pick_layer_index(n_hidden_blocks_plus_emb: int, explicit: Optional[int]) -> int:
    """``n_hidden_blocks_plus_emb`` = len(hidden_states[0]) for one generate step."""
    n = int(n_hidden_blocks_plus_emb)
    if n <= 0:
        return 0
    if explicit is not None:
        li = int(explicit)
        return max(0, min(li, n - 1))
    return max(1, min(n - 1, int(0.6 * float(n - 1))))


class SigmaHIDE:
    """HIDE-style score: official upstream metric when deps exist; otherwise lab HSIC."""

    def __init__(
        self,
        *,
        backend: str = "auto",
        layer: Optional[int] = None,
        keywords: int = 20,
        kernel: str = "rbf",
        sigma_kernel: Optional[float] = None,
        layers: Optional[List[int]] = None,
    ):
        b = (backend or "auto").strip().lower()
        if b not in ("auto", "official", "lab"):
            raise ValueError("backend must be one of: auto, official, lab")
        self._backend_req = b
        self.layer = layer
        self.keywords = int(keywords)
        if kernel != "rbf":
            raise ValueError("only kernel='rbf' is supported for the official upstream path")
        self.kernel = kernel
        self.sigma_kernel = sigma_kernel
        # Lab backend only: if non-empty, HSIC is averaged over these block indices (embedding = 0).
        self.layers = layers

    def _effective_backend(self) -> str:
        if self._backend_req == "lab":
            return "lab"
        if self._backend_req == "official":
            return "official" if _upstream_deps_ok() else "lab"
        return "official" if _upstream_deps_ok() else "lab"

    def effective_backend(self) -> str:
        return str(self._effective_backend())

    def _lab_encode_split(
        self,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        max_length: int,
    ) -> Tuple[Dict[str, Any], int]:
        p = (prompt or "").strip()
        r = (response or "").strip()
        text = f"{p}\n{r}".strip() if r else p
        if not text:
            raise RuntimeError("lab HIDE: empty prompt/response after strip")
        enc_full = tokenizer(
            text,
            return_tensors="pt",
            truncation=True,
            max_length=int(max_length),
            add_special_tokens=True,
        )
        enc_prompt = tokenizer(
            p,
            return_tensors="pt",
            truncation=True,
            max_length=int(max_length),
            add_special_tokens=True,
        )
        full_ids = enc_full["input_ids"][0].tolist()
        p_ids = enc_prompt["input_ids"][0].tolist()
        split = 0
        for i in range(min(len(full_ids), len(p_ids))):
            if full_ids[i] != p_ids[i]:
                break
            split = i + 1
        if split == 0 and len(full_ids) > 1:
            split = 1
        if split >= len(full_ids):
            split = max(1, len(full_ids) - 1)
        return enc_full, int(split)

    def _lab_hsic_norm_at_layer(
        self,
        hs_tup: Tuple[Any, ...],
        split: int,
        layer_idx: int,
    ) -> Tuple[float, float, float, int, int]:
        """
        HSIC + normalized dependence at one transformer index for the lab token split.
        Returns ``(nh, hsic, rbf_bw, n_pair, layer_idx_clamped)``.
        """
        nl = len(hs_tup)
        li = max(1, min(int(layer_idx), nl - 1))
        h = hs_tup[li].detach().float().cpu().numpy()[0]
        if h.ndim != 2:
            raise RuntimeError("lab HIDE: expected hidden states [seq, dim]")

        seq = int(h.shape[0])
        split_use = min(split, seq - 1)
        split_use = max(1, split_use)
        n_in = split_use
        n_out = seq - split_use
        n_pair = min(n_in, n_out)
        min_n = 2
        if n_pair < min_n:
            raise RuntimeError(
                "lab HIDE: need at least "
                f"{min_n} paired prompt/continuation tokens; got n_in={n_in} n_out={n_out}"
            )

        x = h[split_use - n_pair : split_use]
        y = h[split_use : split_use + n_pair]

        kx, bw = _rbf_kernel(x, self.sigma_kernel)
        ky, _ = _rbf_kernel(y, bw)
        hsic = _hsic_unbiased_zero_diag(kx, ky)
        if not math.isfinite(hsic):
            raise RuntimeError("lab HIDE: non-finite HSIC")

        kt = np.array(kx, dtype=np.float64, copy=True)
        lt = np.array(ky, dtype=np.float64, copy=True)
        np.fill_diagonal(kt, 0.0)
        np.fill_diagonal(lt, 0.0)
        fro = math.sqrt(float(np.sum(kt * kt) * np.sum(lt * lt))) + 1e-12
        nh = float(np.clip(hsic / fro, 0.0, 1.0))
        return nh, float(hsic), float(bw), int(n_pair), int(li)

    def _lab_compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        max_length: int = 512,
    ) -> Tuple[float, Dict[str, Any]]:
        import torch

        enc_full, split = self._lab_encode_split(
            tokenizer, prompt, response, max_length=max_length
        )
        dev = next(model.parameters()).device
        enc_full = {k: v.to(dev) for k, v in enc_full.items()}
        with torch.no_grad():
            out = model(**enc_full, output_hidden_states=True)
        hs_tup = out.hidden_states
        if not hs_tup:
            raise RuntimeError("lab HIDE: model returned no hidden_states")

        nl = len(hs_tup)
        layer_list: Optional[List[int]] = self.layers
        if layer_list and len(layer_list) > 0:
            indices = sorted({max(1, min(int(li), nl - 1)) for li in layer_list})
            per: List[Dict[str, Any]] = []
            nhs: List[float] = []
            for li in indices:
                nh, hsic, bw, n_pair, li_c = self._lab_hsic_norm_at_layer(hs_tup, split, li)
                nhs.append(nh)
                per.append(
                    {
                        "layer": int(li_c),
                        "hsic": hsic,
                        "normalized_hsic": nh,
                        "rbf_sigma": bw,
                        "n_pairs": n_pair,
                    }
                )
            mean_nh = float(np.mean(np.asarray(nhs, dtype=np.float64)))
            sigma = float(1.0 - mean_nh)
            best_i = int(np.argmax(np.asarray(nhs, dtype=np.float64)))
            best = per[best_i]
            details: Dict[str, Any] = {
                "split_tokens": int(split),
                "layers_scanned": [int(p["layer"]) for p in per],
                "per_layer": per,
                "best_layer": int(best["layer"]),
                "mean_normalized_hsic": mean_nh,
                "n_pairs": int(best["n_pairs"]),
                "hsic": float(best["hsic"]),
                "normalized_hsic": float(best["normalized_hsic"]),
                "rbf_sigma": float(best["rbf_sigma"]),
                "sigma_hide": sigma,
                "backend": "lab",
                "method": "HSIC_token_multi_layer_lab",
            }
            return sigma, details

        layer_idx = _pick_layer_index(nl, self.layer)
        nh, hsic, bw, n_pair, li_c = self._lab_hsic_norm_at_layer(hs_tup, split, layer_idx)
        sigma = float(1.0 - nh)

        details = {
            "split_tokens": int(split),
            "layer": int(li_c),
            "n_pairs": int(n_pair),
            "hsic": float(hsic),
            "normalized_hsic": nh,
            "rbf_sigma": float(bw),
            "sigma_hide": sigma,
            "backend": "lab",
            "method": "HSIC_token_middle_lab",
        }
        return sigma, details

    def _official_compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        max_new_tokens: int,
    ) -> Tuple[float, Dict[str, Any]]:
        import torch

        from .hide_metric_upstream import get_unbiased_hsic_score_keybert

        device = next(model.parameters()).device
        enc = tokenizer((prompt or "").strip(), return_tensors="pt")
        enc = {k: v.to(device) for k, v in enc.items()}
        input_length = int(enc["input_ids"].shape[1])
        mnt = max(1, int(max_new_tokens))

        with torch.no_grad():
            dict_outputs = model.generate(
                **enc,
                num_beams=1,
                do_sample=False,
                max_new_tokens=mnt,
                pad_token_id=tokenizer.pad_token_id,
                output_hidden_states=True,
                return_dict_in_generate=True,
            )
        hidden_states = dict_outputs.hidden_states
        sequences = dict_outputs.sequences
        if not hidden_states or hidden_states[0] is None:
            raise RuntimeError("missing hidden_states from generate()")

        nl = len(hidden_states[0])
        layer_idx = _pick_layer_index(nl, self.layer)
        input_tokens = sequences.detach().cpu()[0, 0:input_length]
        most_likely_generations = sequences.detach().cpu()[0, input_length:]
        hsic_score, _in_kw, _out_kw, _in_tok_t, _out_tok_t = get_unbiased_hsic_score_keybert(
            hidden_states,
            tokenizer,
            input_tokens,
            most_likely_generations,
            keywords=int(self.keywords),
            layer=int(layer_idx),
            kernel=str(self.kernel),
        )
        gen_text = tokenizer.decode(
            most_likely_generations,
            skip_special_tokens=True,
        ).strip()
        ref_text = (response or "").strip()
        sigma = float(1.0 / (1.0 + np.exp(min(20.0, max(-20.0, 10.0 * float(hsic_score))))))
        details: Dict[str, Any] = {
            "hsic": float(hsic_score),
            "sigma_hide": sigma,
            "backend": "official",
            "layer": int(layer_idx),
            "n_generate_steps": len(hidden_states),
            "n_layers_per_step": nl,
            "regenerated_text_preview": gen_text[:500],
            "regeneration_matches_reference": bool(gen_text == ref_text),
            "method": "HIDE_upstream_unbiased_hsic_keybert",
        }
        return sigma, details

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        max_new_tokens: int = 100,
        max_length: int = 512,
    ) -> Tuple[float, Dict[str, Any]]:
        be = self._effective_backend()
        if be == "official":
            return self._official_compute_sigma(
                model,
                tokenizer,
                prompt,
                response,
                max_new_tokens=max_new_tokens,
            )
        return self._lab_compute_sigma(model, tokenizer, prompt, response, max_length=max_length)

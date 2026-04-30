# SPDX-License-Identifier: MIT
#
# NOTICE — upstream provenance
# The functions in this module are copied or closely derived from the public
# reference implementation **HIDE** (Hallucination Detection via Decoupled Representations):
#   https://github.com/C-anwoy/HIDE
# Primary upstream paths: ``func/kernels.py`` (``rbf_kernel``) and ``func/metric.py``
# (``unbiased_HSIC``, ``extract_keyword_representation``, ``compute_unbiased_hsic_with_keywords``,
# ``get_unbiased_hsic_score_keybert``).
#
# In-tree changes (Creation OS portability):
#   * No ``import _settings`` — KeyBERT uses a Hugging Face sentence-transformers id by default.
#   * ``print`` calls in keyword fallback paths are removed (silent fallbacks).
#   * ``_as_int_list`` helper accepts ``torch.Tensor`` or Python lists for token ids.
#
# This is **not** a claim of numerical parity with any paper table — run local calibration.

from __future__ import annotations

import os
from typing import Any, List, Sequence, Tuple

import torch
from keybert import KeyBERT
from sklearn.feature_extraction.text import CountVectorizer


def rbf_kernel(X: torch.Tensor, Y: torch.Tensor | None = None, gamma: float = 1e-7) -> torch.Tensor:
    """Radial Basis Function (RBF) kernel (upstream ``func/kernels.py``)."""
    if Y is None:
        Y = X
    pairwise_sq_dists = torch.cdist(X, Y) ** 2
    return torch.exp(-gamma * pairwise_sq_dists)


KERNEL_FUNCTIONS: dict[str, Any] = {"rbf": rbf_kernel}


def unbiased_HSIC(K_X: torch.Tensor, K_Y: torch.Tensor) -> torch.Tensor:
    """Unbiased HSIC (upstream ``func/metric.py``)."""
    tK = K_X - torch.diag(torch.diag(K_X))
    tL = K_Y - torch.diag(torch.diag(K_Y))

    n = K_X.shape[0]

    hsic = (
        torch.trace(tK @ tL)
        + (torch.sum(tK) * torch.sum(tL) / n**2)
        - (2 * torch.sum(tK @ tL) / n)
    )

    return hsic / n**2


def _as_int_list(tokens: Any) -> List[int]:
    if hasattr(tokens, "tolist"):
        return [int(x) for x in tokens.tolist()]
    return [int(x) for x in list(tokens)]


def extract_keyword_representation(
    X: torch.Tensor,
    Y: torch.Tensor,
    tokenizer: Any,
    input_tokens: Any,
    output_tokens: Any,
    k: int = 20,
) -> Tuple[torch.Tensor, torch.Tensor, list, list, list, list]:
    """Keyword-based token selection (upstream ``func/metric.py``)."""
    keybert_model = os.environ.get(
        "HIDE_KEYBERT_MODEL",
        "sentence-transformers/all-MiniLM-L6-v2",
    )
    kw_model = KeyBERT(model=keybert_model)

    input_text = tokenizer.decode(input_tokens, skip_special_tokens=True)
    output_text = tokenizer.decode(output_tokens, skip_special_tokens=True)

    vectorizer = CountVectorizer(
        ngram_range=(1, 1),
        stop_words=None,
        token_pattern=r"(?u)\b\w+\b",
        min_df=1,
        vocabulary=None,
        lowercase=False,
        max_features=None,
    )
    try:
        input_keywords = kw_model.extract_keywords(
            input_text,
            keyphrase_ngram_range=(1, 1),
            top_n=k,
            use_mmr=True,
            diversity=1,
            vectorizer=vectorizer,
        )
    except Exception:
        input_keywords = []
    try:
        output_keywords = kw_model.extract_keywords(
            output_text,
            keyphrase_ngram_range=(1, 1),
            top_n=k,
            use_mmr=True,
            diversity=1,
            vectorizer=vectorizer,
        )
    except Exception:
        output_keywords = []

    def find_all_occurrences(keyword: str, token_sequence: Sequence[int], tokenizer: Any) -> List[int]:
        variations = [keyword, " " + keyword]
        indices: List[int] = []
        for variant in variations:
            keyword_tokens = tokenizer.encode(variant, add_special_tokens=False)
            if len(keyword_tokens) <= len(token_sequence):
                for i in range(len(token_sequence) - len(keyword_tokens) + 1):
                    if list(token_sequence[i : i + len(keyword_tokens)]) == list(keyword_tokens):
                        indices.extend(range(i, i + len(keyword_tokens)))
        return indices

    input_indices: List[int] = []
    input_tokens_l = _as_int_list(input_tokens)
    output_tokens_l = _as_int_list(output_tokens)
    for keyword, _ in input_keywords:
        keyword_indices = find_all_occurrences(keyword, input_tokens_l, tokenizer)
        if keyword_indices:
            input_indices.extend(keyword_indices)

    output_indices: List[int] = []
    for keyword, _ in output_keywords:
        keyword_indices = find_all_occurrences(keyword, output_tokens_l, tokenizer)
        if keyword_indices:
            output_indices.extend(keyword_indices)

    if not input_indices:
        input_indices = list(range(min(k, len(input_tokens_l))))
    if not output_indices:
        output_indices = list(range(min(k, len(output_tokens_l))))

    k = min(k, len(input_indices), len(output_indices))
    if len(input_indices) > k:
        input_indices = input_indices[:k]
    if len(output_indices) > k:
        output_indices = output_indices[:k]

    selected_input = [input_tokens_l[i] for i in input_indices]
    selected_output = [output_tokens_l[i] for i in output_indices]
    input_tokens_topk = [tokenizer.decode([token]) for token in selected_input]
    output_tokens_topk = [tokenizer.decode([token]) for token in selected_output]

    X_keywords = X[input_indices, :]
    Y_keywords = Y[output_indices, :]

    return X_keywords, Y_keywords, input_keywords, output_keywords, input_tokens_topk, output_tokens_topk


def compute_unbiased_hsic_with_keywords(
    X: torch.Tensor,
    Y: torch.Tensor,
    tokenizer: Any,
    input_tokens: Any,
    output_tokens: Any,
    kernel_func_X: Any,
    kernel_func_Y: Any,
    k: int = 20,
    **kwargs: Any,
) -> Tuple[torch.Tensor, list, list, list, list]:
    """Unbiased HSIC on keyword-selected rows (upstream ``func/metric.py``)."""
    X_keywords, Y_keywords, input_keywords, output_keywords, input_tokens_topk, output_tokens_topk = (
        extract_keyword_representation(X, Y, tokenizer, input_tokens, output_tokens, k=k)
    )

    K_X = kernel_func_X(X_keywords, **kwargs)
    K_Y = kernel_func_Y(Y_keywords, **kwargs)

    hsic = unbiased_HSIC(K_X, K_Y)

    return hsic, input_keywords, output_keywords, input_tokens_topk, output_tokens_topk


def get_unbiased_hsic_score_keybert(
    hidden_states: Any,
    tokenizer: Any,
    input_tokens: Any,
    output_tokens: Any,
    keywords: int,
    layer: int,
    kernel: str = "rbf",
    **kwargs: Any,
) -> Tuple[float, Any, Any, Any, Any]:
    """Greedy-decoding hidden-state layout (upstream ``func/metric.py``)."""
    selected_layer = int(layer)
    selected_states = [token_tuple[selected_layer] for token_tuple in hidden_states]

    X = selected_states[0][0, :, :]
    X = X.to(torch.float32)

    try:
        Y = torch.cat(selected_states[1:], dim=0)[:, 0, :]
    except Exception:
        return 0.0, 0, 0, 0, 0
    Y = Y.to(torch.float32)

    kernel_X = KERNEL_FUNCTIONS[kernel]
    kernel_Y = KERNEL_FUNCTIONS[kernel]
    hsic_score, input_keywords, output_keywords, input_tokens_topk, output_tokens_topk = (
        compute_unbiased_hsic_with_keywords(
            X,
            Y,
            tokenizer,
            input_tokens,
            output_tokens[:-1],
            kernel_X,
            kernel_Y,
            k=keywords,
            **kwargs,
        )
    )

    return float(hsic_score), input_keywords, output_keywords, input_tokens_topk, output_tokens_topk


def upstream_metric_dependencies_available() -> bool:
    try:
        import keybert  # noqa: F401
        import torch  # noqa: F401

        return True
    except Exception:
        return False

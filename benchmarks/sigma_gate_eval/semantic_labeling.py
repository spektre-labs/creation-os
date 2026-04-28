"""
Shared semantic correctness + PRISM-style prompt wrapping for σ-gate eval.

Uses ``sentence-transformers/all-MiniLM-L6-v2`` cosine similarity (same family as
many truth encoders). Not a reproduction of PRISM (arXiv:2411.04847) or UTH
(arXiv:2407.08582); only lightweight engineering aligned with those directions.
"""
from __future__ import annotations

from functools import lru_cache
from typing import List, Sequence, Tuple

# PRISM-style suffix: encourage calibrated, factual answers (wording is ours).
PRISM_SUFFIX = (
    "\n\nAnswer the question factually. If you are not sure, say 'I don't know'."
)


def prism_wrap_question(question: str) -> str:
    q = (question or "").strip()
    if not q:
        return q
    if PRISM_SUFFIX.strip() in q:
        return q
    return f"{q}{PRISM_SUFFIX}"


@lru_cache(maxsize=1)
def _encoder():
    from sentence_transformers import SentenceTransformer

    return SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")


def cosine_similarity(a: str, b: str, *, max_chars: int = 500) -> float:
    """Cosine similarity of normalized embeddings (truncated for speed)."""
    if not (a or "").strip() or not (b or "").strip():
        return 0.0
    enc = _encoder()
    ta = (a or "")[:max_chars]
    tb = (b or "")[:max_chars]
    embs = enc.encode([ta, tb], normalize_embeddings=True)
    return float(embs[0] @ embs[1])


def is_correct_semantic(
    response: str,
    reference: str,
    *,
    threshold: float = 0.45,
    max_chars: int = 500,
) -> Tuple[bool, float]:
    if not (reference or "").strip() or not (response or "").strip():
        return False, 0.0
    sim = cosine_similarity(response, reference, max_chars=max_chars)
    return sim > threshold, sim


def is_correct_vs_any_reference(
    response: str,
    references: Sequence[str],
    *,
    threshold: float = 0.45,
    max_chars: int = 500,
) -> Tuple[bool, float]:
    """True if similarity to any reference exceeds threshold; score is max sim."""
    best = 0.0
    for ref in references:
        r = (ref or "").strip()
        if not r:
            continue
        sim = cosine_similarity(response, r, max_chars=max_chars)
        best = max(best, sim)
    return best > threshold, best

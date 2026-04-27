# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""
Semantic Embedding Uncertainty (SEU): 1 − mean pairwise cosine similarity
of sentence embeddings (diagnostic; optional ``sentence-transformers``).
"""

from __future__ import annotations

from typing import List, Optional

_model = None


def _get_model():
    global _model
    if _model is None:
        from sentence_transformers import SentenceTransformer

        name = "sentence-transformers/all-MiniLM-L6-v2"
        _model = SentenceTransformer(name)
    return _model


def compute_seu(responses: List[str]) -> Optional[float]:
    """Return SEU in [0, 1], or ``None`` if deps missing or fewer than two strings."""
    texts = [(t or "").strip() for t in responses if (t or "").strip()]
    if len(texts) < 2:
        return 0.5
    try:
        import numpy as np
    except ImportError:
        return None
    try:
        model = _get_model()
        emb = model.encode(texts, normalize_embeddings=True)
        if not isinstance(emb, np.ndarray):
            emb = np.asarray(emb, dtype=np.float64)
    except Exception:
        return None
    n = int(emb.shape[0])
    if n < 2:
        return 0.5
    sim = emb @ emb.T
    mask = ~np.eye(n, dtype=bool)
    mean_sim = float(sim[mask].mean())
    return float(np.clip(1.0 - mean_sim, 0.0, 1.0))

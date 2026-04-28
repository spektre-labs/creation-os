"""
Training-free spectral features from **self-attention** matrices (graph view).

Motivation (orientation only — not a line-by-line port of any paper):
  * **LapEigvals**-style attention graphs and Laplacian eigenstructure (upstream:
    ``external/lapeig/`` if cloned; see ``hallucinations/features/laplacian.py``).
  * **EigenTrack**-style cross-layer drift of simple spectral summaries.
  * **Attention sink** mass on early / special tokens (engineering proxy).
  * **Ensemble-of-signals** framing (e.g. internal-representation ensembles) is a
    separate literature strand; do not merge lab AUROCs with harness headlines.

**Causal fast path:** for strictly **lower-triangular** autoregressive attention ``A``,
the **combinatorial** Laplacian ``L = D - A`` (``D = diag(A 1)``) is lower triangular,
so its eigenvalues are exactly ``L_{ii} = (A 1)_i - A_{ii}`` — **O(n)** per head with
no eigendecomposition. This does **not** equal eigenvalues of the **symmetric
normalized** Laplacian used in ``"normalized_symmetric"`` mode; pick one backend per
experiment and do not claim numeric parity across backends.

Scalars are for fusion experiments — validate before external claims.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Tuple

import numpy as np


def _symmetric_adjacency(attn_avg: np.ndarray) -> np.ndarray:
    """Undirected adjacency for a stable Laplacian (mean of A and A^T)."""
    a = np.asarray(attn_avg, dtype=np.float64)
    return 0.5 * (a + a.T)


def _layer_spectral_features(attn_avg: np.ndarray) -> Dict[str, float]:
    a = _symmetric_adjacency(attn_avg)
    deg = a.sum(axis=1)
    deg = np.maximum(deg, 1e-8)
    d = np.diag(deg)
    l = d - a
    inv_sqrt = np.diag(1.0 / np.sqrt(deg))
    l_norm = inv_sqrt @ l @ inv_sqrt
    try:
        w = np.linalg.eigvalsh(l_norm)
    except np.linalg.LinAlgError:
        return {}
    w = np.sort(np.clip(w, 0.0, None))
    if w.size < 2:
        return {}
    spectral_gap = float(w[1] - w[0])
    fiedler = float(w[1])
    lam_max = float(w[-1])
    pos = w[w > 1e-12]
    if pos.size > 0:
        p = pos / pos.sum()
        spectral_entropy = float(-np.sum(p * np.log(p + 1e-12)))
    else:
        spectral_entropy = 0.0
    return {
        "spectral_gap": spectral_gap,
        "fiedler": fiedler,
        "lambda_max": lam_max,
        "spectral_entropy": spectral_entropy,
    }


def compute_attention_spectrum(attention_matrices: List[np.ndarray]) -> Dict[str, float]:
    """
    ``attention_matrices``: list of (n_heads, seq, seq) or (seq, seq) arrays / tensors.

    Returns flat keys ``layer_{i}_*`` for downstream logging / fusion.
    """
    features: Dict[str, float] = {}
    for layer_idx, attn in enumerate(attention_matrices):
        arr = np.asarray(attn, dtype=np.float64)
        if arr.ndim == 3:
            attn_avg = arr.mean(axis=0)
        elif arr.ndim == 2:
            attn_avg = arr
        else:
            continue
        lf = _layer_spectral_features(attn_avg)
        for k, v in lf.items():
            features[f"layer_{layer_idx}_{k}"] = float(v)
    return features


def spectral_sigma(
    model: Any,
    tokenizer: Any,
    prompt: str,
    response: str,
    *,
    max_length: int = 512,
) -> Tuple[float, Dict[str, float]]:
    """
    Aggregate spectral statistics into a single ``sigma_spectral`` in [0, 1].

    Heuristic: map mean normalized Laplacian spectral entropy (per layer) through
    a mild affine + clip. Replace with task-tuned calibration if you publish.
    """
    import torch

    text = f"{(prompt or '').strip()} {(response or '').strip()}".strip()
    if not text:
        return 0.5, {}

    inputs = tokenizer(text, return_tensors="pt", truncation=True, max_length=int(max_length))
    dev = next(model.parameters()).device
    inputs = {k: v.to(dev) for k, v in inputs.items()}

    with torch.no_grad():
        outputs = model(**inputs, output_attentions=True)

    attns = outputs.attentions
    if not attns:
        return 0.5, {}

    mats: List[np.ndarray] = []
    for t in attns:
        x = t.detach().float().cpu().numpy()
        if x.ndim == 4:
            x = x[0]
        mats.append(x)

    feats = compute_attention_spectrum(mats)
    entropies = [v for k, v in feats.items() if k.endswith("_spectral_entropy")]
    if not entropies:
        return 0.5, feats

    mean_se = float(np.mean(entropies))
    n = max(2, int(mats[0].shape[-1]))
    scale = float(np.log(n + 2.0))
    sigma = float(np.clip(mean_se / max(scale, 1e-6), 0.0, 1.0))
    return sigma, feats


def _symmetrize_attn(attn: np.ndarray) -> np.ndarray:
    a = np.asarray(attn, dtype=np.float64)
    return 0.5 * (a + a.T)


def causal_combinatorial_laplacian_eigenvalues_sorted(attn_matrix: np.ndarray) -> np.ndarray:
    """
    Full sorted spectrum of **combinatorial** ``L = D - A`` when ``A`` is causal
    (lower triangular after ``np.tril``).

    For lower-triangular ``L``, eigenvalues are the diagonal entries ``L_{ii}``;
    we return ``np.sort(diag(L))`` for stable downstream summaries.
    """
    a = np.tril(np.asarray(attn_matrix, dtype=np.float64))
    degree = a.sum(axis=1)
    lap_diag = degree - np.diag(a)
    return np.sort(np.asarray(lap_diag, dtype=np.float64))


def _normalized_laplacian_eigenvalues_symmetric(a_sym: np.ndarray) -> np.ndarray:
    """
    Symmetric normalized Laplacian eigenvalues (ascending).

    L = I - D^{-1/2} A D^{-1/2} with A the (symmetrized) non-negative adjacency.
    """
    a_sym = np.asarray(a_sym, dtype=np.float64)
    deg = a_sym.sum(axis=1)
    deg = np.maximum(deg, 1e-8)
    inv_sqrt = np.diag(1.0 / np.sqrt(deg))
    l_norm = np.eye(a_sym.shape[0], dtype=np.float64) - inv_sqrt @ a_sym @ inv_sqrt
    l_norm = 0.5 * (l_norm + l_norm.T)
    try:
        return np.sort(np.linalg.eigvalsh(l_norm))
    except np.linalg.LinAlgError:
        return np.array([], dtype=np.float64)


class SpectralSigma:
    """
    Laplacian top-k eigenvalues, spectral entropy, sink mass, and cross-layer
    aggregates (EigenTrack-style drift on summaries).

    Optional ``sklearn`` logistic regression on a **flat numeric feature dict**
    (``train_probe``) — not shipped calibrated; train offline on your labels.
    """

    def __init__(
        self,
        k: int = 10,
        layers: Optional[List[int]] = None,
        *,
        laplacian_backend: str = "normalized_symmetric",
    ):
        self.k = int(k)
        self.layers = layers
        if laplacian_backend not in ("normalized_symmetric", "causal_combinatorial_fast"):
            raise ValueError(
                "laplacian_backend must be 'normalized_symmetric' or 'causal_combinatorial_fast'"
            )
        self.laplacian_backend = str(laplacian_backend)
        self.probe: Any = None
        self.scaler: Any = None
        self._probe_feature_keys: Optional[List[str]] = None

    def extract_attention(self, model: Any, tokenizer: Any, text: str, *, max_length: int = 512) -> List[Any]:
        """Return list of (n_heads, seq, seq) tensors, one per layer."""
        import torch

        inputs = tokenizer(text, return_tensors="pt", truncation=True, max_length=int(max_length))
        dev = next(model.parameters()).device
        inputs = {kk: vv.to(dev) for kk, vv in inputs.items()}
        with torch.no_grad():
            outputs = model(**inputs, output_attentions=True)
        attns = outputs.attentions
        if not attns:
            return []
        out: List[Any] = []
        for t in attns:
            x = t
            if hasattr(x, "detach"):
                x = x.detach().float()
            if x.dim() == 4:
                x = x[0]
            out.append(x)
        return out

    @staticmethod
    def compute_laplacian_eigenvalues_fast(attn_matrix: np.ndarray) -> np.ndarray:
        """Alias for ``causal_combinatorial_laplacian_eigenvalues_sorted`` (O(n))."""
        return causal_combinatorial_laplacian_eigenvalues_sorted(attn_matrix)

    def compute_laplacian_eigenvalues(self, attn_matrix: np.ndarray, k: Optional[int] = None) -> np.ndarray:
        """Top-``k`` largest eigenvalues (see ``laplacian_backend``)."""
        kk = int(k) if k is not None else self.k
        if self.laplacian_backend == "causal_combinatorial_fast":
            w = causal_combinatorial_laplacian_eigenvalues_sorted(attn_matrix)
        else:
            a_sym = _symmetrize_attn(attn_matrix)
            w = _normalized_laplacian_eigenvalues_symmetric(a_sym)
        if w.size == 0:
            return np.zeros(kk, dtype=np.float64)
        take = min(kk, w.size)
        return np.asarray(w[-take:], dtype=np.float64)

    @staticmethod
    def compute_sink_score(attn_matrix: np.ndarray) -> float:
        """Mean attention mass **to** the first token column (sink proxy)."""
        a = np.asarray(attn_matrix, dtype=np.float64)
        if a.ndim != 2 or a.shape[1] < 1:
            return 0.0
        return float(np.mean(a[:, 0]))

    @staticmethod
    def compute_spectral_entropy(eigvals: np.ndarray) -> float:
        v = np.maximum(np.asarray(eigvals, dtype=np.float64), 1e-10)
        p = v / v.sum()
        return float(-np.sum(p * np.log(p + 1e-10)))

    @staticmethod
    def compute_spectral_gap(eigvals: np.ndarray) -> float:
        """Smallest positive eigenvalue gap (Fiedler-style) on provided spectrum."""
        w = np.sort(np.asarray(eigvals, dtype=np.float64))
        nz = w[w > 1e-6]
        if nz.size < 2:
            return 0.0
        return float(nz[1] - nz[0])

    def compute_eigentrack_features(
        self,
        eigvals_per_layer: List[np.ndarray],
        *,
        gap_per_layer_full: Optional[List[float]] = None,
    ) -> Dict[str, float]:
        """
        Cross-layer drift on **top-k** spectral entropy; gap statistics prefer
        ``gap_per_layer_full`` (Fiedler-style gap on the **full** Laplacian spectrum
        per layer) when provided.
        """
        if not eigvals_per_layer:
            return {
                "entropy_mean": 0.0,
                "entropy_std": 0.0,
                "entropy_drift": 0.0,
                "gap_mean": 0.0,
                "gap_std": 0.0,
                "gap_min": 0.0,
                "gap_drift": 0.0,
            }
        entropies = [self.compute_spectral_entropy(e) for e in eigvals_per_layer]
        if gap_per_layer_full is not None and len(gap_per_layer_full) > 0:
            gaps = [float(x) for x in gap_per_layer_full]
        else:
            gaps = [self.compute_spectral_gap(e) for e in eigvals_per_layer]
        return {
            "entropy_mean": float(np.mean(entropies)),
            "entropy_std": float(np.std(entropies)),
            "entropy_drift": float(entropies[-1] - entropies[0]) if len(entropies) > 1 else 0.0,
            "gap_mean": float(np.mean(gaps)),
            "gap_std": float(np.std(gaps)),
            "gap_min": float(np.min(gaps)) if gaps else 0.0,
            "gap_drift": float(gaps[-1] - gaps[0]) if len(gaps) > 1 else 0.0,
        }

    def compute_all_features(self, model: Any, tokenizer: Any, prompt: str, response: str) -> Dict[str, float]:
        text = f"{(prompt or '').strip()} {(response or '').strip()}".strip()
        if not text:
            return {}

        attentions = self.extract_attention(model, tokenizer, text)
        if not attentions:
            return {}

        n_layers = len(attentions)
        layers_to_use = list(self.layers) if self.layers is not None else list(range(n_layers))

        all_eigvals_topk: List[np.ndarray] = []
        all_sinks: List[float] = []
        gap_full_series: List[float] = []
        all_features: Dict[str, float] = {}

        for layer_idx in layers_to_use:
            if layer_idx < 0 or layer_idx >= n_layers:
                continue
            attn = attentions[layer_idx]
            import torch

            if isinstance(attn, torch.Tensor):
                attn_np = attn.float().cpu().numpy()
            else:
                attn_np = np.asarray(attn, dtype=np.float64)
            if attn_np.ndim != 3:
                continue
            n_heads, _, _ = attn_np.shape
            attn_avg = attn_np.mean(axis=0)

            head_eigvals: List[np.ndarray] = []
            for h in range(n_heads):
                head_eigvals.append(self.compute_laplacian_eigenvalues(attn_np[h], k=self.k))
            mean_eigvals = np.mean(np.stack(head_eigvals, axis=0), axis=0)
            all_eigvals_topk.append(mean_eigvals)

            if self.laplacian_backend == "causal_combinatorial_fast":
                full_eigs = causal_combinatorial_laplacian_eigenvalues_sorted(attn_avg)
            else:
                full_eigs = _normalized_laplacian_eigenvalues_symmetric(_symmetrize_attn(attn_avg))
            gap_full = self.compute_spectral_gap(full_eigs)
            gap_full_series.append(float(gap_full))
            all_features[f"layer_{layer_idx}_spectral_gap_full"] = float(gap_full)
            all_features[f"layer_{layer_idx}_top_eigval"] = float(mean_eigvals[-1]) if mean_eigvals.size else 0.0

            sink = self.compute_sink_score(attn_avg)
            all_sinks.append(sink)
            all_features[f"layer_{layer_idx}_sink"] = float(sink)
            all_features[f"layer_{layer_idx}_spectral_entropy_topk"] = float(self.compute_spectral_entropy(mean_eigvals))

        eigentrack = self.compute_eigentrack_features(
            all_eigvals_topk,
            gap_per_layer_full=gap_full_series or None,
        )
        all_features.update(eigentrack)
        if all_sinks:
            all_features["mean_sink"] = float(np.mean(all_sinks))
            all_features["max_sink"] = float(np.max(all_sinks))
        return all_features

    def _feature_vector(self, features: Dict[str, float]) -> np.ndarray:
        keys = self._probe_feature_keys
        if not keys:
            raise RuntimeError(
                "SpectralSigma: train_probe(..., feature_keys=[...]) must run before probe scoring."
            )
        vec = np.array([float(features.get(k, 0.0)) for k in keys], dtype=np.float64)
        return vec.reshape(1, -1)

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
    ) -> Tuple[float, Dict[str, float]]:
        features = self.compute_all_features(model, tokenizer, prompt, response)
        if not features:
            return 0.5, {}

        if self.probe is not None and self.scaler is not None and self._probe_feature_keys is not None:
            xv = self._feature_vector(features)
            xs = self.scaler.transform(xv)
            sigma = float(self.probe.predict_proba(xs)[0, 1])
            return float(np.clip(sigma, 0.0, 1.0)), features

        entropy_drift = abs(float(features.get("entropy_drift", 0.0)))
        mean_sink = float(features.get("mean_sink", 0.0))
        sigma = float(np.clip(0.5 * (entropy_drift / 2.0) + 0.5 * mean_sink, 0.0, 1.0))
        return sigma, features

    def train_probe(
        self,
        X: np.ndarray,
        y: np.ndarray,
        *,
        feature_keys: Optional[List[str]] = None,
    ) -> float:
        """
        Fit logistic regression on design matrix ``X`` (one row per example).

        ``feature_keys`` length must equal ``X.shape[1]``; same order is used when
        building feature dicts for ``compute_sigma`` with a trained probe.
        """
        from sklearn.linear_model import LogisticRegression
        from sklearn.metrics import roc_auc_score
        from sklearn.preprocessing import StandardScaler

        X = np.asarray(X, dtype=np.float64)
        y = np.asarray(y, dtype=np.int64).ravel()
        n = X.shape[1]
        if feature_keys is not None:
            if len(feature_keys) != n:
                raise ValueError("feature_keys length must match X.shape[1]")
            self._probe_feature_keys = list(feature_keys)
        else:
            self._probe_feature_keys = [f"f{i}" for i in range(n)]

        self.scaler = StandardScaler()
        Xs = self.scaler.fit_transform(X)
        self.probe = LogisticRegression(max_iter=2000)
        self.probe.fit(Xs, y)
        prob = self.probe.predict_proba(Xs)[:, 1]
        return float(roc_auc_score(y, prob))

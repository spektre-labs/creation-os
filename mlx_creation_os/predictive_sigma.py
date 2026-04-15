# -*- coding: utf-8 -*-
"""
Predictive σ (Phase 25) — ennustevirhe hyperavaruudessa.

Friston-tyylinen idea: "vapaa energia" proxy = etäisyys ennustetun ja havaittujen
HV:ien välillä (ei täyttä FEP-mallia — mitattava cos → σ).

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List, Sequence

from ane_hdc_resonance import _hv_encode_py, HDC_DIM
from gda_hdc_bridge import cosine_similarity_hv, distance_sigma_from_cosine


def predictive_sigma(
    predicted_context: str,
    observed_input: str,
) -> Dict[str, Any]:
    pred = _hv_encode_py(predicted_context.strip().lower(), HDC_DIM)
    obs = _hv_encode_py(observed_input.strip().lower(), HDC_DIM)
    cos = cosine_similarity_hv(pred, obs)
    sig = distance_sigma_from_cosine(cos)
    return {
        "hdc_dim": HDC_DIM,
        "cosine_pred_obs": round(cos, 6),
        "sigma_prediction_error": round(sig, 6),
        "interpretation": "low_sigma" if sig < 0.15 else "surprise",
    }


def free_energy_proxy(
    predicted_vec: Sequence[int],
    observed_vec: Sequence[int],
) -> float:
    """Suora vektoripolku (esim. forkatuista haaroista)."""
    cos = cosine_similarity_hv(predicted_vec, observed_vec)
    return float(distance_sigma_from_cosine(cos))

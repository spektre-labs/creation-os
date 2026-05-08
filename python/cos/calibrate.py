# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ calibration: map raw σ to an estimated P(error) via Platt scaling or isotonic fit.

Pure Python (no sklearn). Use :class:`SigmaCalibrator` on validation pairs
``(raw_sigma, is_error)`` where ``is_error`` is 1 if the model was wrong, else 0.
ECE measures alignment of predicted error probability vs empirical error rate.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import json
import math
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

Pair = Tuple[float, float]


def _logistic(z: float) -> float:
    z = max(-60.0, min(60.0, z))
    return 1.0 / (1.0 + math.exp(-z))


def load_calibration_pairs(path: Union[str, Path]) -> List[Pair]:
    """Load ``[[σ, is_error], …]`` or ``[{"sigma":…,"is_error":…}, …]`` from JSON."""
    p = Path(path)
    raw = json.loads(p.read_text(encoding="utf-8"))
    out: List[Pair] = []
    if not raw:
        return out
    if isinstance(raw[0], (list, tuple)):
        for row in raw:
            r = list(row)
            out.append((float(r[0]), float(r[1])))
    else:
        for row in raw:
            d = dict(row)
            out.append((float(d["sigma"]), float(d["is_error"])))
    return out


class SigmaCalibrator:
    """Fit ``σ → P(error)`` with Platt (1-D logistic) or isotonic regression (PAVA)."""

    def __init__(self, method: str = "platt") -> None:
        self.method = str(method)
        self.fitted: bool = False
        self.platt_a: float = 1.0
        self.platt_b: float = 0.0
        self.isotonic_map: List[Tuple[float, float]] = []

    def fit(self, data: Sequence[Pair]) -> None:
        if not data:
            self.fitted = False
            return
        sigmas = [float(d[0]) for d in data]
        labels = [float(d[1]) for d in data]
        if self.method == "platt":
            self._fit_platt(sigmas, labels)
        elif self.method == "isotonic":
            self._fit_isotonic(sigmas, labels)
        else:
            self.method = "platt"
            self._fit_platt(sigmas, labels)
        self.fitted = True

    def calibrate(self, raw_sigma: float) -> float:
        if not self.fitted:
            return float(raw_sigma)
        x = float(raw_sigma)
        if self.method == "platt":
            return float(self._platt_transform(x))
        if self.method == "isotonic":
            return float(self._isotonic_transform(x))
        return x

    def ece(self, data: Optional[Sequence[Pair]] = None, n_bins: int = 10) -> Optional[float]:
        if data is None:
            return None
        rows = list(data)
        if not rows:
            return 0.0
        n_bins = max(1, int(n_bins))
        bins: List[List[Pair]] = [[] for _ in range(n_bins)]
        for raw_sigma, is_error in rows:
            cal_sigma = self.calibrate(float(raw_sigma))
            cal_sigma = max(0.0, min(1.0, cal_sigma))
            idx = min(int(cal_sigma * n_bins), n_bins - 1)
            bins[idx].append((cal_sigma, float(is_error)))

        total = sum(len(b) for b in bins)
        if total == 0:
            return 0.0
        ece = 0.0
        for b in bins:
            if not b:
                continue
            avg_conf = sum(s for s, _ in b) / len(b)
            avg_err = sum(e for _, e in b) / len(b)
            ece += (len(b) / total) * abs(avg_conf - avg_err)
        return float(ece)

    def _fit_platt(self, sigmas: List[float], labels: List[float]) -> None:
        a, b = 1.0, 0.0
        lr = 0.05
        n = len(sigmas)
        if n == 0:
            return
        for _ in range(2000):
            ga, gb = 0.0, 0.0
            for sigma, label in zip(sigmas, labels):
                z = a * sigma + b
                p = _logistic(z)
                err = p - float(label)
                ga += err * sigma
                gb += err
            a -= lr * ga / n
            b -= lr * gb / n
        self.platt_a = float(a)
        self.platt_b = float(b)

    def _platt_transform(self, sigma: float) -> float:
        z = self.platt_a * sigma + self.platt_b
        return _logistic(z)

    def _fit_isotonic(self, sigmas: List[float], labels: List[float]) -> None:
        """PAVA on sorted points; piecewise-constant non-decreasing fit."""
        pairs = sorted(zip(sigmas, labels))
        blocks: List[List[Tuple[float, float]]] = [[(float(x), float(y))] for x, y in pairs]

        def block_avg(b: List[Tuple[float, float]]) -> float:
            return sum(y for _, y in b) / len(b)

        i = 0
        while i < len(blocks) - 1:
            if block_avg(blocks[i]) > block_avg(blocks[i + 1]):
                blocks[i].extend(blocks[i + 1])
                del blocks[i + 1]
                if i > 0:
                    i -= 1
            else:
                i += 1

        self.isotonic_map = []
        for b in blocks:
            x_max = max(x for x, _ in b)
            p = sum(y for _, y in b) / len(b)
            self.isotonic_map.append((x_max, float(p)))

    def _isotonic_transform(self, sigma: float) -> float:
        if not self.isotonic_map:
            return float(sigma)
        s = float(sigma)
        for x_u, p in self.isotonic_map:
            if s <= x_u:
                return float(max(0.0, min(1.0, p)))
        return float(max(0.0, min(1.0, self.isotonic_map[-1][1])))

    def save(self, path: Union[str, Path]) -> None:
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "method": self.method,
            "fitted": self.fitted,
            "platt_a": self.platt_a,
            "platt_b": self.platt_b,
            "isotonic_map": self.isotonic_map,
        }
        with p.open("w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2, ensure_ascii=False)

    def load(self, path: Union[str, Path]) -> None:
        p = Path(path)
        with p.open("r", encoding="utf-8") as f:
            data: Dict[str, Any] = json.load(f)
        self.method = str(data.get("method", "platt"))
        self.fitted = bool(data.get("fitted", False))
        self.platt_a = float(data.get("platt_a", 1.0))
        self.platt_b = float(data.get("platt_b", 0.0))
        raw_map = data.get("isotonic_map") or []
        self.isotonic_map = [(float(a), float(b)) for a, b in raw_map]


class CalibrationReport:
    """Reliability bins + ECE summary for a fitted calibrator."""

    def __init__(
        self,
        calibrator: SigmaCalibrator,
        data: Sequence[Pair],
        n_bins: int = 10,
    ) -> None:
        self.calibrator = calibrator
        self.data = list(data)
        self.n_bins = max(1, int(n_bins))

    def generate(self) -> Dict[str, Any]:
        bins: List[Dict[str, List[float]]] = [
            {"predicted": [], "actual": []} for _ in range(self.n_bins)
        ]

        for raw_sigma, is_error in self.data:
            cal = self.calibrator.calibrate(float(raw_sigma))
            cal = max(0.0, min(1.0, cal))
            idx = min(int(cal * self.n_bins), self.n_bins - 1)
            bins[idx]["predicted"].append(cal)
            bins[idx]["actual"].append(float(is_error))

        reliability: List[Dict[str, Any]] = []
        for i, b in enumerate(bins):
            if not b["predicted"]:
                continue
            ap = sum(b["predicted"]) / len(b["predicted"])
            aa = sum(b["actual"]) / len(b["actual"])
            reliability.append(
                {
                    "bin": i,
                    "range": f"{i / self.n_bins:.1f}-{(i + 1) / self.n_bins:.1f}",
                    "avg_predicted": ap,
                    "avg_actual": aa,
                    "count": int(len(b["predicted"])),
                    "gap": abs(ap - aa),
                }
            )

        return {
            "ece": self.calibrator.ece(self.data, self.n_bins),
            "method": self.calibrator.method,
            "n_samples": len(self.data),
            "n_bins": self.n_bins,
            "reliability_diagram": reliability,
        }


def smECE(
    confidences: Sequence[float],
    errors: Sequence[float],
    *,
    n_grid: int = 40,
    bandwidth: Optional[float] = None,
) -> float:
    """Smooth ECE: kernel-weighted calibration gap on a grid (no hard bins)."""
    c_list = [max(0.0, min(1.0, float(x))) for x in confidences]
    e_list = [float(e) for e in errors]
    n = len(c_list)
    if n < 2 or len(e_list) != n:
        return 0.0
    bw = bandwidth if bandwidth is not None else max(0.04, n ** (-0.2))
    n_grid = max(4, int(n_grid))
    total_gap = 0.0
    for k in range(n_grid):
        x = k / max(1, n_grid - 1)
        num_e = 0.0
        den = 0.0
        for c, e in zip(c_list, e_list):
            w = math.exp(-0.5 * ((c - x) / bw) ** 2)
            num_e += w * e
            den += w
        if den <= 1e-12:
            continue
        e_hat = num_e / den
        total_gap += abs(e_hat - x)
    return float(total_gap / n_grid)


def log_snr_accuracy_hallucination(
    accuracy: float,
    hallucination_rate: float,
    *,
    eps: float = 1e-9,
) -> float:
    """Log10 SNR style ratio (accuracy vs hallucination mass); lab scalar, not claimed paper SNR."""
    num = max(eps, float(accuracy))
    den = max(eps, float(hallucination_rate))
    return float(math.log10(num / den))


def true_positive_answered_accuracy(
    records: Sequence[Dict[str, Any]],
    *,
    min_confidence: float = 0.0,
) -> float:
    """Accuracy restricted to non-ABSTAIN rows with ``confidence >= min_confidence``."""
    rows = [
        r
        for r in records
        if r.get("answered") and float(r.get("confidence", 1.0)) >= min_confidence
    ]
    if not rows:
        return 0.0
    return float(sum(1 for r in rows if r.get("correct")) / len(rows))


def false_negative_abstain_rate(records: Sequence[Dict[str, Any]]) -> float:
    """Among ABSTAIN rows, fraction where ``oracle_correct`` is true (should have answered)."""
    abst = [r for r in records if r.get("abstain")]
    if not abst:
        return 0.0
    wrong_abst = sum(1 for r in abst if bool(r.get("oracle_correct")))
    return float(wrong_abst / len(abst))


def behavioral_calibration_table(
    records: Sequence[Dict[str, Any]],
    *,
    n_grid: int = 40,
) -> Dict[str, Any]:
    """Aggregate behavioral / σ rows into M-tier helpers (host-run; not harness claims)."""
    recs = list(records)
    n = max(len(recs), 1)
    answered = [r for r in recs if r.get("answered")]
    acc_answered = (
        sum(1 for r in answered if r.get("correct")) / max(len(answered), 1) if answered else 0.0
    )
    abst_rate = sum(1 for r in recs if r.get("abstain")) / n
    confidences = []
    for r in answered:
        conf = float(r.get("confidence", 1.0 - float(r.get("sigma", 0.5))))
        confidences.append(max(0.0, min(1.0, conf)))
    errors = [0.0 if r.get("correct") else 1.0 for r in answered]
    smooth = smECE(confidences, errors, n_grid=n_grid) if len(confidences) > 1 else 0.0
    halluc_rate = sum(errors) / max(len(errors), 1) if errors else 0.0
    snr = log_snr_accuracy_hallucination(acc_answered, halluc_rate)
    calib_gap = float(smooth + abst_rate * 0.1)
    m_tier = round(acc_answered * (1.0 - abst_rate) * (1.0 - min(1.0, calib_gap)), 6)
    return {
        "accuracy_answered": round(acc_answered, 6),
        "abstention_rate": round(abst_rate, 6),
        "smECE": round(smooth, 6),
        "calibration_gap": round(calib_gap, 6),
        "SNR_log": round(snr, 6),
        "true_positive_answered_accuracy": round(
            true_positive_answered_accuracy(recs),
            6,
        ),
        "false_negative_abstain_rate": round(false_negative_abstain_rate(recs), 6),
        "M_tier_behavioral": m_tier,
        "n": len(recs),
        "disclaimer": "Lab table from σ-bench style rows; do not substitute for harness cards.",
    }


def adaptive_threshold_snr(
    records: Sequence[Dict[str, Any]],
    *,
    taus: Optional[Sequence[float]] = None,
) -> Dict[str, Any]:
    """Grid search abstain threshold on ``sigma`` to maximize behavioral SNR (lite)."""
    base = list(records)
    grid = [float(t) for t in (taus or [i / 40 for i in range(6, 28)])]
    best_tau = 0.35
    best_snr = -1e9
    for tau in grid:
        synth: List[Dict[str, Any]] = []
        for r in base:
            sigma = float(r.get("sigma", 0.5))
            oracle_ok = bool(r.get("oracle_correct", r.get("correct")))
            abst = sigma > tau
            ans = not abst
            # When abstaining, mark correct=False for accuracy path; oracle tracks regret
            out_ok = bool(r.get("correct")) if ans else False
            synth.append(
                {
                    "sigma": sigma,
                    "answered": ans,
                    "abstain": abst,
                    "correct": out_ok,
                    "oracle_correct": oracle_ok,
                    "confidence": max(0.0, min(1.0, 1.0 - sigma)),
                },
            )
        tab = behavioral_calibration_table(synth)
        sn = float(tab["SNR_log"])
        if sn > best_snr:
            best_snr = sn
            best_tau = tau
    return {"threshold_accept_proxy": best_tau, "SNR_log": round(best_snr, 6)}


__all__ = [
    "CalibrationReport",
    "SigmaCalibrator",
    "adaptive_threshold_snr",
    "behavioral_calibration_table",
    "false_negative_abstain_rate",
    "load_calibration_pairs",
    "log_snr_accuracy_hallucination",
    "smECE",
    "true_positive_answered_accuracy",
]

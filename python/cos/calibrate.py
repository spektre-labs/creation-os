# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ calibration: map raw σ to an estimated P(error) via Platt scaling or isotonic fit.

Pure Python (no sklearn). Use :class:`SigmaCalibrator` on validation pairs
``(raw_sigma, is_error)`` where ``is_error`` is 1 if the model was wrong, else 0.
ECE measures alignment of predicted error probability vs empirical error rate.
"""
from __future__ import annotations

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
        bins: List[Dict[str, List[float]]] = [ {"predicted": [], "actual": []} for _ in range(self.n_bins)]

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


__all__ = ["SigmaCalibrator", "CalibrationReport", "load_calibration_pairs"]

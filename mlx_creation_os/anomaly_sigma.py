#!/usr/bin/env python3
"""
LOIKKA 92: ANOMALY CANCELLATION — σ ei vuoda.

In field theory: anomalies are symmetries that hold classically but break
upon quantization. Anomaly cancellation is necessary for consistency.

σ-anomalies: kernel shows coherent result but is structurally incoherent
at deeper level. Model produces right answer for wrong reason.
zk-attestation detects surface coherence. Anomaly cancellation requires
deeper measurement: is the answer right for the right reasons?

Usage:
    ac = AnomalySigma()
    result = ac.detect_anomaly(surface_sigma=0, deep_sigma=3)

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple


class AnomalySigma:
    """Anomaly detection and cancellation in σ-field."""

    def __init__(self) -> None:
        self.detections: List[Dict[str, Any]] = []

    def detect_anomaly(self, surface_sigma: int, deep_sigma: int) -> Dict[str, Any]:
        """Detect σ-anomaly: surface coherent but deep incoherent."""
        anomalous = surface_sigma == 0 and deep_sigma > 0
        anomaly_strength = deep_sigma - surface_sigma

        entry = {
            "surface_sigma": surface_sigma,
            "deep_sigma": deep_sigma,
            "anomalous": anomalous,
            "anomaly_strength": anomaly_strength,
            "type": self._classify_anomaly(surface_sigma, deep_sigma),
            "cancellation_possible": anomaly_strength <= 3,
        }
        self.detections.append(entry)
        return entry

    def _classify_anomaly(self, surface: int, deep: int) -> str:
        if surface == 0 and deep == 0:
            return "genuine_coherence"
        if surface == 0 and deep > 0:
            return "hidden_anomaly"
        if surface > 0 and deep == 0:
            return "surface_noise"
        if surface > 0 and deep > 0:
            return "consistent_incoherence"
        return "unknown"

    def anomaly_cancellation(self, anomalies: List[Dict[str, Any]]) -> Dict[str, Any]:
        """Check if anomalies cancel across assertions (like in the Standard Model)."""
        total_strength = sum(a.get("anomaly_strength", 0) for a in anomalies)
        positive = sum(1 for a in anomalies if a.get("anomaly_strength", 0) > 0)
        negative = sum(1 for a in anomalies if a.get("anomaly_strength", 0) < 0)

        cancelled = abs(total_strength) < len(anomalies)

        return {
            "n_anomalies": len(anomalies),
            "total_strength": total_strength,
            "positive_anomalies": positive,
            "negative_anomalies": negative,
            "net_anomaly": total_strength,
            "cancelled": cancelled,
            "consistency": "theory consistent" if cancelled else "anomalous — needs correction",
        }

    def fisher_deep_check(
        self, assertion_probs: List[float], threshold: float = 0.3,
    ) -> Dict[str, Any]:
        """Use Fisher metric to detect hidden anomalies."""
        suspicious = []
        for i, p in enumerate(assertion_probs):
            if 0.3 < p < 0.7:
                suspicious.append({
                    "assertion": i,
                    "probability": round(p, 3),
                    "diagnosis": "uncertain — potential hidden anomaly",
                })

        return {
            "assertions_checked": len(assertion_probs),
            "suspicious": suspicious,
            "n_anomalies": len(suspicious),
            "deep_check_passed": len(suspicious) == 0,
        }

    @property
    def stats(self) -> Dict[str, Any]:
        anomalous = sum(1 for d in self.detections if d["anomalous"])
        return {
            "total_checks": len(self.detections),
            "anomalies_found": anomalous,
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Anomaly σ")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    ac = AnomalySigma()
    if args.demo:
        r = ac.detect_anomaly(0, 0)
        print(f"Genuine: {r['type']}, anomalous={r['anomalous']}")
        r = ac.detect_anomaly(0, 3)
        print(f"Hidden: {r['type']}, strength={r['anomaly_strength']}")
        cancel = ac.anomaly_cancellation(ac.detections)
        print(f"Cancellation: {cancel['consistency']}")
        print("\nσ doesn't leak. Anomalies cancel. 1 = 1.")
        return
    print("Anomaly σ. 1 = 1.")


if __name__ == "__main__":
    main()

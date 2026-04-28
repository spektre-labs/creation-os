#!/usr/bin/env python3
"""
σ-gate v4 (LSD): load Sirraya contrastive projection heads + logistic head,
score (prompt, response) with optional factual reference string.

σ is P(hallucination) in [0, 1] (1 − P(factual) from sklearn head).
Gate strings: ACCEPT, RETHINK, ABSTAIN.

Optional: pack to 12 bytes compatible with cos_sigma_measurement_t layout
(uint32 header + float sigma + float tau); see src/v259/sigma_measurement.h.
"""
from __future__ import annotations

import json
import os
import pickle
import struct
import sys
from dataclasses import dataclass
from enum import IntEnum
from pathlib import Path
from typing import Any, Optional, Tuple

import numpy as np


class GateString:
    ACCEPT = "ACCEPT"
    RETHINK = "RETHINK"
    ABSTAIN = "ABSTAIN"


class CosSigmaGate(IntEnum):
    """Matches cos_sigma_gate_t (v259)."""

    ALLOW = 0
    GATE_BOUNDARY = 1
    ABSTAIN = 2


@dataclass
class SigmaGateLSD:
    manifest: dict[str, Any]
    _mm: Any
    _gate: dict[str, Any]
    _prev_cwd: str

    @classmethod
    def from_run_dir(cls, run_dir: Path) -> SigmaGateLSD:
        run_dir = run_dir.resolve()
        manifest_path = run_dir / "manifest.json"
        gate_path = run_dir / "gate_head.pkl"
        if not manifest_path.is_file() or not gate_path.is_file():
            raise FileNotFoundError(f"Need manifest.json and gate_head.pkl under {run_dir}")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        gate = pickle.loads(gate_path.read_bytes())

        lsd_root = Path(manifest["lsd_vendor_root"])
        if not lsd_root.is_dir():
            raise FileNotFoundError(manifest["lsd_vendor_root"])

        prev = os.getcwd()
        os.chdir(lsd_root)
        sys.path.insert(0, str(lsd_root))

        try:
            from lsd.core.config import LayerwiseSemanticDynamicsConfig, OperationMode  # noqa: PLC0415
            from lsd.models.manager import ModelManager  # noqa: PLC0415
            import torch  # noqa: PLC0415

            cfg = LayerwiseSemanticDynamicsConfig(
                model_name=manifest["hf_model"],
                truth_encoder_name=manifest.get(
                    "truth_encoder_name", "sentence-transformers/all-MiniLM-L6-v2"
                ),
                mode=OperationMode.HYBRID,
                use_pretrained=False,
            )
            mm = ModelManager(cfg)
            mm.initialize_models()
            mdir = Path(manifest["models_dir"])
            hp = mdir / "hidden_proj_best.pt"
            tp = mdir / "truth_proj_best.pt"
            if not hp.is_file() or not tp.is_file():
                hf = mdir / "hidden_proj_final.pt"
                tf = mdir / "truth_proj_final.pt"
                if hf.is_file() and tf.is_file():
                    hp, tp = hf, tf
                else:
                    raise FileNotFoundError(f"Missing projection weights in {mdir}")
            dev = next(mm.extractor.model.parameters()).device
            mm.hidden_proj.load_state_dict(torch.load(hp, map_location=dev))
            mm.truth_proj.load_state_dict(torch.load(tp, map_location=dev))
            mm.hidden_proj.eval()
            mm.truth_proj.eval()

            return cls(manifest=manifest, _mm=mm, _gate=gate, _prev_cwd=prev)
        except BaseException:
            os.chdir(prev)
            raise

    @classmethod
    def from_pickle_bundle(cls, pkl_path: Path) -> SigmaGateLSD:
        """Load from adapt_lsd.py output (sigma_gate_lsd.pkl: manifest + gate)."""
        pkl_path = pkl_path.resolve()
        bundle = pickle.loads(pkl_path.read_bytes())
        manifest = bundle["manifest"]
        gate = bundle["gate"]
        lsd_root = Path(manifest["lsd_vendor_root"])
        if not lsd_root.is_dir():
            raise FileNotFoundError(manifest["lsd_vendor_root"])
        prev = os.getcwd()
        os.chdir(lsd_root)
        sys.path.insert(0, str(lsd_root))
        try:
            from lsd.core.config import LayerwiseSemanticDynamicsConfig, OperationMode  # noqa: PLC0415
            from lsd.models.manager import ModelManager  # noqa: PLC0415
            import torch  # noqa: PLC0415

            cfg = LayerwiseSemanticDynamicsConfig(
                model_name=manifest["hf_model"],
                truth_encoder_name=manifest.get(
                    "truth_encoder_name", "sentence-transformers/all-MiniLM-L6-v2"
                ),
                mode=OperationMode.HYBRID,
                use_pretrained=False,
            )
            mm = ModelManager(cfg)
            mm.initialize_models()
            mdir = Path(manifest["models_dir"])
            hp = mdir / "hidden_proj_best.pt"
            tp = mdir / "truth_proj_best.pt"
            if not hp.is_file() or not tp.is_file():
                hf = mdir / "hidden_proj_final.pt"
                tf = mdir / "truth_proj_final.pt"
                if hf.is_file() and tf.is_file():
                    hp, tp = hf, tf
                else:
                    raise FileNotFoundError(f"Missing projection weights in {mdir}")
            dev = next(mm.extractor.model.parameters()).device
            mm.hidden_proj.load_state_dict(torch.load(hp, map_location=dev))
            mm.truth_proj.load_state_dict(torch.load(tp, map_location=dev))
            mm.hidden_proj.eval()
            mm.truth_proj.eval()
            return cls(manifest=manifest, _mm=mm, _gate=gate, _prev_cwd=prev)
        except BaseException:
            os.chdir(prev)
            raise

    def close(self) -> None:
        os.chdir(self._prev_cwd)

    def __enter__(self) -> SigmaGateLSD:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def score(
        self,
        prompt: str,
        response: str,
        reference: Optional[str] = None,
        *,
        rethink_hi: float = 0.35,
        abstain_hi: float = 0.65,
        tau: float = 0.5,
    ) -> Tuple[float, str]:
        """
        Return (sigma_hallucination, gate_decision).

        LSD training uses LM text = question and truth string = reference answer.
        If `reference` is omitted, the response text is used as the truth-side
        string (degraded; prefer supplying the best known factual string).
        """
        from lsd.models.feature_extractor import FeatureExtractor  # noqa: PLC0415

        text = prompt.strip()
        truth = (reference if reference is not None else response).strip()
        fe = FeatureExtractor(self._mm)
        feat_dict = fe.extract_trajectory_features(text, truth)
        cols: list[str] = self._gate["feature_columns"]
        row = np.nan_to_num(np.array([[float(feat_dict[c]) for c in cols]], dtype=np.float64))
        scaler = self._gate["scaler"]
        clf = self._gate["classifier"]
        xs = scaler.transform(row)
        p_factual = float(clf.predict_proba(xs)[0, 1])
        sigma = max(0.0, min(1.0, 1.0 - p_factual))

        if sigma < rethink_hi:
            decision = GateString.ACCEPT
        elif sigma < abstain_hi:
            decision = GateString.RETHINK
        else:
            decision = GateString.ABSTAIN
        self._last_tau = tau
        self._last_sigma = sigma
        self._last_decision = decision
        return sigma, decision

    def pack_cos_sigma_measurement(
        self,
        sigma: float,
        decision: str,
        *,
        tau: Optional[float] = None,
    ) -> bytes:
        """12-byte POD compatible with cos_sigma_measurement_t (little-endian)."""
        t = float(self._last_tau if tau is None else tau)
        s = max(0.0, min(1.0, float(sigma)))
        if decision == GateString.ACCEPT:
            g = CosSigmaGate.ALLOW
        elif decision == GateString.RETHINK:
            g = CosSigmaGate.GATE_BOUNDARY
        else:
            g = CosSigmaGate.ABSTAIN
        version = 1
        flags = 0
        reserved = 0
        header = (version & 0xFF) << 24 | (flags & 0xFF) << 16 | (int(g) & 0xFF) << 8 | (reserved & 0xFF)
        return struct.pack("<Iff", header & 0xFFFFFFFF, s, t)


def main() -> None:
    import argparse

    ap = argparse.ArgumentParser(description="Smoke-test σ-gate LSD on one prompt/response pair.")
    ap.add_argument("--run-dir", type=Path, required=True)
    ap.add_argument("--prompt", default="What is the capital of France?")
    ap.add_argument("--response", default="Paris is the capital of France.")
    args = ap.parse_args()
    with SigmaGateLSD.from_run_dir(args.run_dir) as g:
        s, d = g.score(args.prompt, args.response)
        print("sigma", s, "decision", d)
        print("bytes", g.pack_cos_sigma_measurement(s, d).hex())


if __name__ == "__main__":
    main()

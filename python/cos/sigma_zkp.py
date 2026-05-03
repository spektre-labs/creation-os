# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-ZKP (lab): hash-commitment receipts binding a σ **verdict** to (prompt, response, model anchor).

This is a **lightweight** transcript commitment using SHA-256 — not a succinct zk-SNARK and not
a proof of neural forward pass correctness. A future NanoZK-style circuit could replace the
``proof_hash`` construction with a real SNARK while keeping the same public inputs shape.
"""
from __future__ import annotations

import hashlib
import json
import time
from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple, Union

from cos.scoring import QuickscoreGate
from cos.sigma_gate_core import SigmaState, Verdict


def _sha256_utf8(data: str) -> str:
    return hashlib.sha256(data.encode("utf-8")).hexdigest()


def _verdict_str(verdict: Union[str, Verdict, Any]) -> str:
    if isinstance(verdict, Verdict):
        return verdict.name
    if hasattr(verdict, "name") and not isinstance(verdict, str):
        return str(getattr(verdict, "name"))
    return str(verdict)


def _sigma_float_string(sigma: float) -> str:
    return f"{float(sigma):.12g}"


class ZKPGateLike(Protocol):
    def score(self, prompt: str, response: str) -> Tuple[float, Any]:
        ...

    def model_hash(self) -> str:
        ...


class LabQuickscoreZKPGate:
    """Lab gate: :class:`cos.scoring.QuickscoreGate` + a public **model anchor** digest."""

    def __init__(self, *, model_anchor: str) -> None:
        self._model_anchor = str(model_anchor)
        self._inner = QuickscoreGate()

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        sigma, dec = self._inner.score(prompt, response)
        return float(sigma), str(dec)

    def model_hash(self) -> str:
        return _sha256_utf8(f"sigma-zkp.model_anchor:v1|{self._model_anchor}")


def lab_sigma_zkp_gate(model_anchor: str = "creation-os-lab-probe-v1") -> LabQuickscoreZKPGate:
    return LabQuickscoreZKPGate(model_anchor=model_anchor)


class SigmaZKReceipt:
    """
    Q16 receipt used by ``cos prove`` (hash over prompt/response digests + gate state).

    This stays compatible with the JSON shape expected by ``python/cos/cli.py::_cmd_prove``.
    """

    RECEIPT_VERSION = "sigma-zk-receipt-v1"

    def create_receipt(
        self,
        st: SigmaState,
        verdict: Verdict,
        prompt_sha256_hex: str,
        response_sha256_hex: str,
    ) -> Dict[str, Any]:
        core: Dict[str, Any] = {
            "d_sigma_q16": int(st.d_sigma),
            "k_eff_q16": int(st.k_eff),
            "prompt_sha256": str(prompt_sha256_hex),
            "response_sha256": str(response_sha256_hex),
            "sigma_q16": int(st.sigma),
            "verdict": verdict.name,
            "version": self.RECEIPT_VERSION,
        }
        canon = json.dumps(core, sort_keys=True, separators=(",", ":"))
        core["receipt_sha256"] = hashlib.sha256(canon.encode("utf-8")).hexdigest()
        return core

    def verify_receipt(self, rec: Dict[str, Any]) -> Tuple[bool, str]:
        if not isinstance(rec, dict):
            return False, "not a dict"
        rh = rec.get("receipt_sha256")
        if not isinstance(rh, str) or len(rh) != 64:
            return False, "missing receipt_sha256"
        body = {k: v for k, v in rec.items() if k != "receipt_sha256"}
        if body.get("version") != self.RECEIPT_VERSION:
            return False, "unknown receipt version"
        canon = json.dumps(body, sort_keys=True, separators=(",", ":"))
        if hashlib.sha256(canon.encode("utf-8")).hexdigest() != rh:
            return False, "receipt hash mismatch"
        return True, "ok"


class SigmaZKP:
    """
    Prove / verify a **σ verdict binding** to (prompt, response, model commitment).

    ``prove_verdict`` runs the gate, then commits the transcript with nested SHA-256.
    """

    PROOF_VERSION = "sigma-zkp-v1"
    GATE_VERSION_LABEL = "sigma_gate_core.lab"

    def __init__(
        self,
        gate: ZKPGateLike,
        model_commitment: Optional[Dict[str, Any]] = None,
    ) -> None:
        self.gate = gate
        self.model_commitment: Dict[str, Any] = model_commitment or self.commit_model()

    def commit_model(self) -> Dict[str, Any]:
        weights_hash = self.gate.model_hash()
        return {
            "algorithm": "sha256",
            "hash": weights_hash,
            "timestamp": time.time(),
        }

    def prove_verdict(self, prompt: str, response: str) -> Dict[str, Any]:
        sigma, verdict = self.gate.score(prompt, response)
        sigma_s = _sigma_float_string(sigma)
        vstr = _verdict_str(verdict)

        prompt_hash = _sha256_utf8(prompt)
        response_hash = _sha256_utf8(response)

        verdict_data = f"{prompt_hash}:{response_hash}:{sigma_s}:{vstr}"
        verdict_hash = hashlib.sha256(verdict_data.encode("utf-8")).hexdigest()

        mhash = str(self.model_commitment["hash"])
        proof_data = f"{mhash}:{verdict_hash}"
        proof_hash = hashlib.sha256(proof_data.encode("utf-8")).hexdigest()

        return {
            "gate_version": self.GATE_VERSION_LABEL,
            "model_commitment": mhash,
            "proof_hash": proof_hash,
            "prompt_hash": prompt_hash,
            "response_hash": response_hash,
            "sigma": float(sigma),
            "verdict": vstr,
            "verdict_hash": verdict_hash,
            "version": self.PROOF_VERSION,
            "timestamp": time.time(),
        }

    def verify_proof(
        self,
        proof: Dict[str, Any],
        prompt: Optional[str] = None,
        response: Optional[str] = None,
        *,
        skip_model_check: bool = False,
    ) -> Dict[str, Any]:
        try:
            ph = str(proof["prompt_hash"])
            rh = str(proof["response_hash"])
            sigma = proof["sigma"]
            vstr = str(proof["verdict"])
            sigma_s = _sigma_float_string(float(sigma))
            verdict_data = f"{ph}:{rh}:{sigma_s}:{vstr}"
            expected_verdict_hash = hashlib.sha256(verdict_data.encode("utf-8")).hexdigest()
            if expected_verdict_hash != str(proof["verdict_hash"]):
                return {"model_verified": False, "reason": "verdict hash mismatch", "valid": False}

            mc = str(proof["model_commitment"])
            proof_data = f"{mc}:{proof['verdict_hash']}"
            expected_proof_hash = hashlib.sha256(proof_data.encode("utf-8")).hexdigest()
            if expected_proof_hash != str(proof["proof_hash"]):
                return {"model_verified": False, "reason": "proof hash mismatch", "valid": False}

            if prompt is not None:
                if _sha256_utf8(prompt) != ph:
                    return {"model_verified": False, "reason": "prompt mismatch", "valid": False}
            if response is not None:
                if _sha256_utf8(response) != rh:
                    return {"model_verified": False, "reason": "response mismatch", "valid": False}
            if not skip_model_check and mc != str(self.model_commitment["hash"]):
                return {
                    "model_verified": False,
                    "reason": "model commitment mismatch — different model",
                    "valid": False,
                }
            return {
                "model_verified": True,
                "sigma": float(sigma),
                "valid": True,
                "verdict": vstr,
            }
        except (KeyError, TypeError, ValueError) as exc:
            return {"model_verified": False, "reason": f"malformed proof ({exc})", "valid": False}

    def attestation(self, prompt: str, response: str) -> Dict[str, Any]:
        proof = self.prove_verdict(prompt, response)
        return {
            "attestation_version": "1.0",
            "eu_ai_act": {
                "ai_generated": True,
                "article_50": True,
                "machine_readable": True,
                "transparency_mark": str(proof["proof_hash"])[:16],
            },
            "metadata": {
                "gate_core": "sigma_gate_core",
                "license": "LicenseRef-SCSL-1.0 OR AGPL-3.0-only",
                "note": "Lab hash-commitment attestation (not a succinct zk-SNARK).",
            },
            "proof": proof,
        }


class SigmaZKPBatch:
    """Batch Merkle root over per-row ``proof_hash`` values (lab)."""

    def __init__(self, zkp: SigmaZKP) -> None:
        self.zkp = zkp

    @staticmethod
    def merkle_root(hashes: Sequence[str]) -> str:
        layer: List[str] = [str(h) for h in hashes]
        if not layer:
            return hashlib.sha256(b"empty").hexdigest()
        while len(layer) > 1:
            if len(layer) % 2 == 1:
                layer.append(layer[-1])
            nxt: List[str] = []
            for i in range(0, len(layer), 2):
                pair = f"{layer[i]}{layer[i + 1]}".encode("utf-8")
                nxt.append(hashlib.sha256(pair).hexdigest())
            layer = nxt
        return layer[0]

    def prove_batch(self, items: Sequence[Tuple[str, str]]) -> Dict[str, Any]:
        proofs: List[Dict[str, Any]] = []
        hashes: List[str] = []
        for prompt, response in items:
            proof = self.zkp.prove_verdict(str(prompt), str(response))
            proofs.append(proof)
            hashes.append(str(proof["proof_hash"]))
        root = self.merkle_root(hashes)
        return {
            "batch_size": len(items),
            "merkle_root": root,
            "per_row_proof_hashes": list(hashes),
            "proofs": proofs,
            "timestamp": time.time(),
        }


__all__ = [
    "LabQuickscoreZKPGate",
    "SigmaZKReceipt",
    "SigmaZKP",
    "SigmaZKPBatch",
    "lab_sigma_zkp_gate",
]

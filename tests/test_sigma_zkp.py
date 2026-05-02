# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Tests for σ-ZKP lab (hash commitments) and ``cos zkp`` CLI."""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

_REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_REPO / "python"))

from cos.sigma_gate_core import SigmaState, sigma_gate, sigma_q16, sigma_update  # noqa: E402
from cos.sigma_zkp import SigmaZKReceipt, SigmaZKP, SigmaZKPBatch, lab_sigma_zkp_gate  # noqa: E402


def test_sigma_zkp_prove_verify_valid() -> None:
    gate = lab_sigma_zkp_gate("model-alpha")
    zkp = SigmaZKP(gate)
    p = zkp.prove_verdict("What is 2+2?", "4")
    v = SigmaZKP(lab_sigma_zkp_gate("model-alpha"), model_commitment=zkp.model_commitment)
    r = v.verify_proof(p, "What is 2+2?", "4")
    assert r["valid"] is True
    assert r.get("model_verified") is True


def test_sigma_zkp_verify_fails_on_tampered_prompt() -> None:
    zkp = SigmaZKP(lab_sigma_zkp_gate("model-alpha"))
    proof = zkp.prove_verdict("What is 2+2?", "4")
    v = SigmaZKP(lab_sigma_zkp_gate("model-alpha"), model_commitment=zkp.model_commitment)
    r = v.verify_proof(proof, "What is 2+3?", "4")
    assert r["valid"] is False
    assert "prompt" in str(r.get("reason", "")).lower()


def test_sigma_zkp_model_commitment_mismatch() -> None:
    zkp_a = SigmaZKP(lab_sigma_zkp_gate("model-a"))
    zkp_b = SigmaZKP(lab_sigma_zkp_gate("model-b"))
    proof = zkp_a.prove_verdict("p", "r")
    res = zkp_b.verify_proof(proof, "p", "r")
    assert res["valid"] is False
    assert "model" in str(res.get("reason", "")).lower()


def test_sigma_zkp_batch_merkle_and_row_verify() -> None:
    zkp = SigmaZKP(lab_sigma_zkp_gate("batch-model-v1"))
    items = [(f"prompt-{i}", f"response-{i}") for i in range(100)]
    batch = SigmaZKPBatch(zkp).prove_batch(items)
    assert batch["batch_size"] == 100
    assert len(batch["merkle_root"]) == 64
    row0 = batch["proofs"][0]
    ok = zkp.verify_proof(row0, items[0][0], items[0][1])
    assert ok["valid"] is True


def test_sigma_zk_receipt_round_trip() -> None:
    z = SigmaZKReceipt()
    st = SigmaState()
    st.sigma = sigma_q16(0.06)
    sigma_update(st, 0.06, 0.92)
    verdict = sigma_gate(st)
    import hashlib

    ph = hashlib.sha256(b"cli smoke").hexdigest()
    rh = hashlib.sha256(b"cli smoke").hexdigest()
    rec = z.create_receipt(st, verdict, ph, rh)
    ok, msg = z.verify_receipt(rec)
    assert ok and msg == "ok"


def test_cos_zkp_cli_prove_verify_smoke() -> None:
    env = {**__import__("os").environ, "PYTHONPATH": str(_REPO / "python")}
    pr = subprocess.run(
        [
            sys.executable,
            "-m",
            "cos",
            "zkp",
            "--prove",
            "--prompt",
            "What is 2+2?",
            "--response",
            "4",
            "--model-anchor",
            "cli-zkp-anchor",
        ],
        cwd=str(_REPO),
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    assert pr.returncode == 0, pr.stderr
    proof = json.loads(pr.stdout.strip())
    assert proof.get("version") == "sigma-zkp-v1"
    mc = str(proof["model_commitment"])

    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False, encoding="utf-8") as t:
        t.write(json.dumps(proof))
        t.flush()
        proof_path = t.name
    try:
        vr2 = subprocess.run(
            [
                sys.executable,
                "-m",
                "cos",
                "zkp",
                "--verify",
                "--proof",
                proof_path,
                "--prompt",
                "What is 2+2?",
                "--response",
                "4",
                "--expected-commitment",
                mc,
                "--model-anchor",
                "cli-zkp-anchor",
            ],
            cwd=str(_REPO),
            env=env,
            capture_output=True,
            text=True,
            check=False,
        )
        assert vr2.returncode == 0, vr2.stderr
        body = json.loads(vr2.stdout.strip())
        assert body["valid"] is True
    finally:
        Path(proof_path).unlink(missing_ok=True)

#!/usr/bin/env python3
"""
Diverse Verifier Tree Search (DVTS) + σ-kernel.

K independent samples at diverse temperatures; σ-kernel (receipt) scores each;
lowest σ wins. If every candidate exceeds a threshold, run one InftyThink-backed
refinement pass on the best draft, then re-score.

Designed for small proposer (e.g. 3B) + strong verifier (σ / coherence / firmware).
Aligns with diverse-verifier search narratives: multiple proposals, single verifier
picks the structurally best under σ (HF / literature: small proposer + search).

1 = 1.
"""
from __future__ import annotations

import os
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

# Research-style temperature ladder (wider than single-beam).
DVTS_TEMPERATURES: Tuple[float, ...] = (0.3, 0.5, 0.7, 0.9, 1.1)


def _sigma_value(receipt: Dict[str, Any]) -> int:
    v = receipt.get("sigma_after")
    if v is None:
        v = receipt.get("sigma")
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def compact_verifier_vector(receipt: Dict[str, Any], text: str) -> Dict[str, Any]:
    """Structured σ-side features for audit (firmware, coherence, epistemics, lw, halts)."""
    trace = receipt.get("sigma_per_token") or []
    tail = trace[-32:] if len(trace) > 32 else trace
    return {
        "sigma": _sigma_value(receipt),
        "coherent": bool(receipt.get("coherent")),
        "firmware_detected": bool(receipt.get("firmware_detected")),
        "epistemic_accepted": bool(receipt.get("epistemic_accepted", True)),
        "halted_firmware_leak": bool(receipt.get("halted_firmware_leak")),
        "living_weights_updates": int(receipt.get("living_weights_updates", 0) or 0),
        "token_steps": len(trace),
        "token_sigma_trace_tail": tail,
        "semantic_sigma": receipt.get("semantic_sigma"),
        "tier_used": receipt.get("tier_used"),
        "mixture_sigma_tier": receipt.get("mixture_sigma_tier"),
        "text_len": len(text or ""),
    }


@dataclass
class DvtsResult:
    text: str
    sigma: int
    receipt: Dict[str, Any]
    winner_temperature: float
    candidates: List[Dict[str, Any]] = field(default_factory=list)
    infty_refinement_used: bool = False
    audit_trail: List[Dict[str, Any]] = field(default_factory=list)


def _pick_winner(rows: List[Dict[str, Any]]) -> int:
    """Index of winning candidate: lowest σ, tie-break coherent + shorter."""
    best_i = 0
    best_s = 10**9
    for i, r in enumerate(rows):
        s = int(r.get("sigma", 0))
        coh = bool(r.get("coherent"))
        ln = int(r.get("text_len", 0))
        if s < best_s:
            best_i, best_s = i, s
            continue
        if s == best_s:
            prev_coh = bool(rows[best_i].get("coherent"))
            if coh and not prev_coh:
                best_i = i
            elif coh == prev_coh and ln < int(rows[best_i].get("text_len", 0)):
                best_i = i
    return best_i


def _infty_refine(
    base_prompt: str,
    draft: str,
    *,
    max_tokens: int,
    top_p: float,
) -> Tuple[str, Dict[str, Any]]:
    """One refinement generation with Phase27 / InftyThink defaults enabled."""
    from creation_os import creation_os_generate_native

    saved: Dict[str, Optional[str]] = {}
    keys = (
        "CREATION_OS_PHASE27",
        "CREATION_OS_INFTY_THINK",
        "CREATION_OS_KERNEL_SMART_INFERENCE",
        "CREATION_OS_INFTY_SEGMENT_TOKENS",
        "CREATION_OS_SEMANTIC_SIGMA",
    )
    for k in keys:
        saved[k] = os.environ.get(k)
    try:
        os.environ["CREATION_OS_PHASE27"] = "1"
        os.environ.setdefault("CREATION_OS_INFTY_THINK", "1")
        os.environ.setdefault("CREATION_OS_KERNEL_SMART_INFERENCE", "1")
        os.environ.setdefault("CREATION_OS_INFTY_SEGMENT_TOKENS", "128")
        os.environ.setdefault("CREATION_OS_SEMANTIC_SIGMA", "1")
        rp = (
            base_prompt.rstrip()
            + "\n\n### Refinement\nImprove the draft for correctness and coherence. "
            "Obey the original format (e.g. one letter A–D on the first line for multiple-choice).\n\n"
            "### Draft\n"
            + (draft or "")[:4000]
        )
        return creation_os_generate_native(rp, max_tokens=max_tokens, temp=0.2, top_p=top_p)
    finally:
        for k, v in saved.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v


def dvts_generate(
    prompt: str,
    *,
    max_tokens: int = 256,
    top_p: float = 0.95,
    model_path: Optional[str] = None,
    temperatures: Tuple[float, ...] = DVTS_TEMPERATURES,
    sigma_all_bad_threshold: Optional[int] = None,
    enable_infty_on_all_bad: bool = True,
    post_audit: Optional[Callable[[str, Dict[str, Any], Dict[str, Any]], None]] = None,
) -> DvtsResult:
    """
    Run DVTS: one generation per temperature, σ-score via native receipt, pick lowest σ.

    ``sigma_all_bad_threshold``: if all candidates have σ strictly greater than this,
    run InftyThink refinement once. Default: ``CREATION_OS_DVTS_SIGMA_THRESHOLD`` or 8.

    ``post_audit``: optional callback(final_text, final_receipt, dvts_meta_dict).
    """
    from creation_os import creation_os_generate_native

    thr = sigma_all_bad_threshold
    if thr is None:
        raw = os.environ.get("CREATION_OS_DVTS_SIGMA_THRESHOLD", "").strip()
        thr = int(raw) if raw.isdigit() else 8

    run_id = str(uuid.uuid4())
    trail: List[Dict[str, Any]] = [{"event": "dvts_start", "run_id": run_id, "k": len(temperatures)}]
    rows: List[Dict[str, Any]] = []
    receipts: List[Dict[str, Any]] = []

    for t in temperatures:
        text, receipt = creation_os_generate_native(
            prompt,
            model_path=model_path,
            max_tokens=max_tokens,
            temp=float(t),
            top_p=top_p,
        )
        vec = compact_verifier_vector(receipt, text)
        vec.update({"temperature": t, "text": text})
        rows.append(vec)
        receipts.append(dict(receipt))
        trail.append(
            {
                "event": "candidate",
                "temperature": t,
                "verifier": {k: v for k, v in vec.items() if k != "text"},
                "text_head": (text or "")[:240],
            }
        )

    idx = _pick_winner(rows)
    winner = rows[idx]
    text_out = str(winner.get("text") or "")
    receipt_out = dict(receipts[idx])
    receipt_out["dvts_run_id"] = run_id
    receipt_out["dvts_winner_temperature"] = float(winner["temperature"])

    all_bad = all(int(r.get("sigma", 0)) > thr for r in rows)
    infty_used = False
    if all_bad and enable_infty_on_all_bad:
        trail.append({"event": "infty_trigger", "reason": "all_sigma_gt_threshold", "threshold": thr})
        text_r, rec_r = _infty_refine(prompt, text_out, max_tokens=max_tokens, top_p=top_p)
        text_out = text_r
        receipt_out = dict(rec_r)
        receipt_out["dvts_run_id"] = run_id
        receipt_out["dvts_infty_refinement"] = True
        infty_used = True
        trail.append(
            {
                "event": "infty_done",
                "sigma": _sigma_value(rec_r),
                "text_head": (text_out or "")[:240],
            }
        )

    meta = {
        "run_id": run_id,
        "sigma_threshold": thr,
        "infty_refinement_used": infty_used,
        "candidates": [{k: v for k, v in r.items() if k != "text"} for r in rows],
    }
    if post_audit is not None:
        post_audit(text_out, receipt_out, meta)

    _audit = os.environ.get("CREATION_OS_SIGMA_AUDIT_LOG", "").strip()
    if _audit:
        try:
            from sigma_audit_log import append_from_generation

            append_from_generation(
                Path(_audit),
                prompt=prompt,
                output_text=text_out,
                receipt=receipt_out,
                extra={"pipeline": "dvts", "infty_refinement": infty_used, "dvts_run_id": run_id},
            )
        except OSError:
            pass

    candidates_public = [{k: v for k, v in r.items() if k != "text"} for r in rows]

    return DvtsResult(
        text=text_out,
        sigma=_sigma_value(receipt_out),
        receipt=receipt_out,
        winner_temperature=float(winner["temperature"]),
        candidates=candidates_public,
        infty_refinement_used=infty_used,
        audit_trail=trail,
    )

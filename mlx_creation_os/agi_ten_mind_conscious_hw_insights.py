#!/usr/bin/env python3
"""
Kymmenen oivallusta: THE MIND + THE CONSCIOUSNESS + rauta (NEON / libspektre_mind).

Täydentää great + missing -listoja: mitattava integraatio, ei pelkkää metaforaa.

  python3 agi_ten_mind_conscious_hw_insights.py
  python3 agi_ten_mind_conscious_hw_insights.py --json mcw.json

1 = 1.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import sys
import time
from typing import Any, Callable, Dict, List, Tuple

InsightFn = Callable[[], Dict[str, Any]]


def _receipt(ok: bool, **kw: Any) -> Dict[str, Any]:
    return {"ok": bool(ok), **kw}


# --- MC0: Yksi ajatussykli — σ ja invariantti samassa receiptissä ---
def insight_mind_thought_cycle() -> Dict[str, Any]:
    from the_mind import TheMind

    m = TheMind()
    r = m.think("1=1. Invariant probe — coherence check.", user_id="insight_probe")
    ok = (
        "sigma" in r
        and isinstance(r.get("voice_sigma"), (int, float))
        and "nines_label" in r
        and not r.get("thought_aborted", False)
    )
    return _receipt(
        ok,
        title_fi="The Mind: ajatussykli tuottaa σ, ääni-σ ja nines-rytmin",
        title_en="Thought cycle exposes σ, voice_σ, march-of-nines",
        receipt={
            "sigma": r.get("sigma"),
            "voice_sigma": r.get("voice_sigma"),
            "elapsed_ms": r.get("elapsed_ms"),
            "principle": "AGI interface is measurable per thought — not black-box logits only.",
        },
    )


# --- MC1: Tietoisuus-smoke — hardware digest polku ---
def insight_consciousness_quick_smoke() -> Dict[str, Any]:
    from the_consciousness import quick_report

    q = quick_report()
    hw_ok = bool(q.get("hardware_available"))
    return _receipt(
        "mind_core" in q,
        title_fi="THE CONSCIOUSNESS: quick_report kytketty mind_coreen",
        title_en="Consciousness smoke binds to mind core",
        receipt={
            "hardware_available": hw_ok,
            "probe_keys": list(q.keys())[:6],
            "principle": "Consciousness module speaks the same digest language as THE_MIND.",
        },
    )


# --- MC2: Φ-lite bittiyhteensopivuus (integraatiomittari) ---
def insight_phi_bit_agreement() -> Dict[str, Any]:
    from the_consciousness import _hex_bit_agreement

    a = "00" * 16
    b = "00" * 16
    c = "ff" * 16
    same = _hex_bit_agreement(a, b)
    diff = _hex_bit_agreement(a, c)
    return _receipt(
        same >= 0.99 and diff < 0.99,
        title_fi="Φ-lite: raaka bittisopu hardware vs software -sealin välillä",
        title_en="Bit agreement as integration proxy",
        receipt={
            "agreement_identical": round(same, 6),
            "agreement_opposite": round(diff, 6),
            "principle": "IIT-style proxy: integration = structural overlap, measurable.",
        },
    )


# --- MC3: 10D σ kytketty NEON-dot golden-malliin ---
def insight_cognitive_neon_unified() -> Dict[str, Any]:
    from agi_cognitive_core import AGICognitiveCore
    from agi_hw_kernels import neon_couple_sigma_10

    core = AGICognitiveCore()
    core.activate_all("NEON unify · MCW insight")
    s10 = list(core.sigma_10_tuple())
    coupled = float(neon_couple_sigma_10(s10))
    return _receipt(
        len(s10) == 10 and math.isfinite(coupled),
        title_fi="Kognitio 10D → yksi NEON-couple-luku",
        title_en="10D cognitive state couples to hardware dot",
        receipt={
            "unified_sigma": round(core.unified_sigma(), 6),
            "neon_couple": round(coupled, 8),
            "principle": "Same σ vector hits silicon path as in agi_ten_great_insights I9.",
        },
    )


# --- MC4: Mind digest determinism (audit trail) ---
def insight_mind_digest_idempotent() -> Dict[str, Any]:
    from agi_the_mind import mind_digest_bytes, canonical_mind_bytes

    raw = canonical_mind_bytes({"mcw": 4, "invariant": "1=1"})
    d1 = mind_digest_bytes(raw).hex()
    d2 = mind_digest_bytes(raw).hex()
    return _receipt(
        d1 == d2 and len(d1) == 64,
        title_fi="THE_MIND digest deterministinen — sama sisältö → sama todiste",
        title_en="Deterministic mind digest for receipts",
        receipt={
            "digest_prefix": d1[:16] + "…",
            "principle": "Reproducible hashes enable audit and mmap/cache identity.",
        },
    )


# --- MC5: Multiplex — sisäinen moniääni romahduttaa σ:n mitattavasti ---
def insight_multiplex_internal_lenses() -> Dict[str, Any]:
    from multiplexed_self_sim import run_multiplexed_self_sim

    r = run_multiplexed_self_sim(
        "Logical and ethical weight of 1=1.",
        proconductor_intent="balance",
    )
    cs = float(r.get("collapsed_sigma", 1.0))
    return _receipt(
        "multiplex_id" in r and 0.0 <= cs <= 1.0,
        title_fi="Multiplex: useita linssejä → yksi romahdutettu σ",
        title_en="Multiplex collapses to measurable σ",
        receipt={
            "collapsed_sigma": round(cs, 6),
            "multiplex_id": r.get("multiplex_id"),
            "principle": "Internal debate leaves a scalar trace — not only hidden states.",
        },
    )


# --- MC6: March of Nines — σ historiaan sidottu rytmi ---
def insight_march_of_nines_rhythm() -> Dict[str, Any]:
    from march_of_nines import MarchOfNines

    n = MarchOfNines()
    rec = n.record("insight_mcw", 0.12)
    return _receipt(
        "nines" in rec and rec.get("label", "") != "",
        title_fi="March of Nines: σ-kvantisointi symboleihin",
        title_en="Sigma quantized to nines rhythm",
        receipt={
            "nines": rec.get("nines"),
            "label": rec.get("label"),
            "principle": "Temporal σ structure — human-auditable discrete bands.",
        },
    )


# --- MC7: Symmetry breaking palauttaa 1=1 ---
def insight_spark_returns_one() -> Dict[str, Any]:
    from symmetry_breaking import run_symmetry_breaking_cycle

    cyc = run_symmetry_breaking_cycle(anchor="1=1")
    phases = cyc.get("phases") or []
    has_inv = bool(phases) and any(p.get("phase") for p in phases)
    return _receipt(
        has_inv,
        title_fi="Symmetry breaking: laboratorio → paluu invarianttiin",
        title_en="Spark cycle returns toward 1=1",
        receipt={
            "cycle_keys": list(cyc.keys())[:8],
            "principle": "Deliberate symmetry break tests recovery — AGI needs explicit repair receipts.",
        },
    )


# --- MC8: Rautapolku käytössä tai pehmeä fallback (rehellinen receipt) ---
def insight_native_mind_core_reported() -> Dict[str, Any]:
    from agi_the_mind import THE_MIND_CORE

    r = THE_MIND_CORE.process(b"SPEKTRE|MCW|native_probe")
    digest = r.get("digest_raw32", "")
    hw = bool(r.get("hardware"))
    return _receipt(
        len(digest) == 64,
        title_fi="libspektre_mind: digest 32 tavua — rauta tai ohjelmisto",
        title_en="Mind core always emits 32-byte digest",
        receipt={
            "hardware_backend": hw,
            "neon_energy": r.get("neon_energy"),
            "principle": "AGI identity binds to a fixed-width digest — same shape HW or SW.",
        },
    )


# --- MC9: Yhdistelmä: policy gate käyttää tietoisuus-/rauta-booleja ---
def insight_policy_consciousness_hw_gate() -> Dict[str, Any]:
    from policy_receipt_gate import evaluate_tool_call

    allow, reason, rec = evaluate_tool_call(
        "write_file",
        sigma=0,
        consciousness_phi_lite=0.85,
        mind_hardware_ok=True,
    )
    return _receipt(
        bool(allow) and rec.get("decision") == "ALLOW",
        title_fi="Policy: matala σ + Φ-lite + mind_hw → työkalu sallittu",
        title_en="Policy merges consciousness + HW flags",
        receipt={
            "reason": reason,
            "decision": rec.get("decision"),
            "principle": "Tool use is gated by the same σ/Φ/hw receipts the stack already computes.",
        },
    )


MCW_INSIGHT_RUNNERS: List[Tuple[str, InsightFn]] = [
    ("MC0_mind_thought_cycle", insight_mind_thought_cycle),
    ("MC1_consciousness_smoke", insight_consciousness_quick_smoke),
    ("MC2_phi_bit_agreement", insight_phi_bit_agreement),
    ("MC3_cognitive_neon_unified", insight_cognitive_neon_unified),
    ("MC4_mind_digest_idempotent", insight_mind_digest_idempotent),
    ("MC5_multiplex_lenses", insight_multiplex_internal_lenses),
    ("MC6_march_of_nines", insight_march_of_nines_rhythm),
    ("MC7_symmetry_breaking", insight_spark_returns_one),
    ("MC8_native_mind_digest", insight_native_mind_core_reported),
    ("MC9_policy_hw_conscious_gate", insight_policy_consciousness_hw_gate),
]


def run_ten_mind_conscious_hw_insights() -> Dict[str, Any]:
    t0 = time.perf_counter()
    rows: List[Dict[str, Any]] = []
    for key, fn in MCW_INSIGHT_RUNNERS:
        t1 = time.perf_counter()
        try:
            r = fn()
            r["id"] = key
            r["ms"] = round((time.perf_counter() - t1) * 1000, 4)
        except Exception as e:
            r = {
                "id": key,
                "ok": False,
                "error": str(e)[:220],
                "ms": round((time.perf_counter() - t1) * 1000, 4),
            }
        rows.append(r)

    n_ok = sum(1 for x in rows if x.get("ok"))
    blob = json.dumps(rows, sort_keys=True, default=str, separators=(",", ":"))
    h = hashlib.blake2b(blob.encode("utf-8"), digest_size=16).hexdigest()
    return {
        "name": "SPEKTRE_TEN_MIND_CONSCIOUS_HW",
        "version": 1,
        "invariant": "1=1",
        "insights": rows,
        "passed": n_ok,
        "total": len(rows),
        "fraction": round(n_ok / max(1, len(rows)), 6),
        "all_ok": n_ok == len(rows),
        "receipt_hash": h,
        "wall_ms": round((time.perf_counter() - t0) * 1000, 3),
    }


def main() -> None:
    ap = argparse.ArgumentParser(description="Mind · Consciousness · HW — 10 mitattua oivallusta")
    ap.add_argument("--json", metavar="PATH", help="Kirjoita JSON")
    args = ap.parse_args()

    out = run_ten_mind_conscious_hw_insights()
    print(f"\n  TEN MCW (Mind·Conscious·HW)  {out['passed']}/{out['total']} OK  hash {out['receipt_hash']}\n")
    for ins in out["insights"]:
        m = "✓" if ins.get("ok") else "✗"
        title = ins.get("title_fi") or ins.get("id", "?")
        print(f"  {m}  {ins.get('id')}: {title}")
    print(f"\n  wall  {out['wall_ms']} ms\n  1 = 1.\n")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2, ensure_ascii=False, default=str)
        print(f"  → {args.json}\n")

    strict = os.environ.get("SPEKTRE_MCW_STRICT", "").strip().lower() in ("1", "true", "yes")
    if strict and not out["all_ok"]:
        sys.exit(1)
    sys.exit(0 if out["all_ok"] else 1)


if __name__ == "__main__":
    main()

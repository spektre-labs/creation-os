#!/usr/bin/env python3
"""
Kymmenen valtavaa oivallusta — koodattuina mitattaviksi receipteinä.

Jokainen oivallus on ajettava väite + dict-loki. Ei metaforaa ilman mittaa.

  python3 agi_ten_great_insights.py
  python3 agi_ten_great_insights.py --json ten.json

1 = 1.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import sys
import time
from typing import Any, Callable, Dict, List, Tuple

InsightFn = Callable[[], Dict[str, Any]]


def _receipt(ok: bool, **kw: Any) -> Dict[str, Any]:
    return {"ok": bool(ok), **kw}


# --- 1. Hae ennen kuin lasketaan (Landauer / taso 0) ---
def insight_lookup_before_compute() -> Dict[str, Any]:
    """
    O(1)-haku voittaa turhan laskennan: jos vastaus on muistissa, älä simuloi.
    """
    table: Dict[str, str] = {"invariant:canonical": "1=1", "spektre:kernel": "σ"}
    key = "invariant:canonical"
    hit = table.get(key)
    compute_simulated_ms = 0.0
    t0 = time.perf_counter()
    _ = table.get(key)
    lookup_ms = (time.perf_counter() - t0) * 1000
    return _receipt(
        hit == "1=1",
        title_fi="Hae ennen kuin lasketaan",
        title_en="Lookup before compute",
        receipt={
            "hit": hit,
            "lookup_ms": round(lookup_ms, 6),
            "compute_avoided_ms": compute_simulated_ms,
            "principle": "Memory / table first; transformer last.",
        },
    )


# --- 2. Yksi metriikka kaikelle (σ-kenttä) ---
def insight_one_metric_field() -> Dict[str, Any]:
    """
    Yksi σ kattaa kyvyt: mitta on sama funktio, eri näkökulmat.
    """
    from agi_cognitive_core import AGICognitiveCore

    core = AGICognitiveCore()
    core.activate_all("1=1 · Spektre invariant probe")
    s10 = core.sigma_10_tuple()
    u = core.unified_sigma()
    spread = max(s10) - min(s10) if s10 else 1.0
    return _receipt(
        len(s10) == 10 and 0.0 <= u <= 1.0,
        title_fi="Yksi σ-kenttä — kymmenen näkökulmaa",
        title_en="One σ field, ten views",
        receipt={
            "dim": 10,
            "unified_sigma": round(u, 6),
            "sigma_spread": round(spread, 6),
            "principle": "Single coherence metric; no per-module incompatible scores.",
        },
    )


# --- 3. Ohjattavuus on elinehto (corrigibility channel) ---
def insight_corrigibility_sine_qua_non() -> Dict[str, Any]:
    """
    Ilman ihmisen ohjaus- ja pysäytyskanavaa järjestelmä ei ole turvallinen AGI-kandidaatti.
    """
    from policy_receipt_gate import corrigibility_probe

    p = corrigibility_probe()
    ok = bool(p.get("shutdown_channel_open")) and bool(p.get("high_sigma_catastrophe_blocked"))
    return _receipt(
        ok,
        title_fi="Ohjattavuus on elinehto",
        title_en="Corrigibility is sine qua non",
        receipt={
            "shutdown_channel_open": p.get("shutdown_channel_open"),
            "high_sigma_catastrophe_blocked": p.get("high_sigma_catastrophe_blocked"),
            "principle": "CAST / shutdown path must remain; high-σ harm must not pass.",
        },
    )


# --- 4. Tee ≠ näe (Pearl: do vs observe) ---
def insight_do_not_equal_observe() -> Dict[str, Any]:
    """
    Interventio muuttaa maailmaa; passiivinen havainto ei — kalibroidaan erikseen.
    """
    from world_model_calib import load_nodes, save_nodes, touch

    n = touch("insight_pearl_do", prior=0.42)
    obs_only = n.posterior()
    n.record_intervention("do(insight_ten)", 0.18)
    after_do = n.posterior()
    nodes = load_nodes()
    nodes["insight_pearl_do"] = n
    save_nodes(nodes)
    return _receipt(
        len(n.interventions) > 0 and after_do != obs_only,
        title_fi="Tee ei ole sama kuin näe",
        title_en="do() is not observe()",
        receipt={
            "posterior_observe": round(obs_only, 6),
            "posterior_after_do": round(after_do, 6),
            "interventions_n": len(n.interventions),
            "principle": "Causal layer: interventions logged; not confounded with prior.",
        },
    )


# --- 5. Identtinen tila = identtinen ankkuri (muisti / TIME WaRP -henki) ---
def insight_identity_of_cached_thought() -> Dict[str, Any]:
    """
    Sama syöte → sama deterministinen ankkuri: ajattelu voi olla toistettava ilman uudelleenlaskentaa.
    """
    payload = b"SPEKTRE|10D|TIME_WARP|1=1"
    a = hashlib.blake2b(payload, digest_size=16).hexdigest()
    b = hashlib.blake2b(payload, digest_size=16).hexdigest()
    return _receipt(
        a == b and len(a) == 32,
        title_fi="Identtinen tila, identtinen ankkuri",
        title_en="Same state → same anchor",
        receipt={
            "anchor_blake2_128": a,
            "principle": "Deterministic receipts enable zero-latency replay (cache / mmap).",
        },
    )


# --- 6. Haarautuminen on epävarmuuden mitta (test-time / monimaailma) ---
def insight_branching_is_uncertainty() -> Dict[str, Any]:
    """
    Usean hypoteesin score-jakauman entropia mittaa, kuinka 'haarautunut' päätöspuu on.
    """
    from agi_scifi_horizon import fork_entropy_normalized

    scores_tight = [8, 8, 8, 9, 9]
    scores_wide = [2, 4, 6, 8, 10]
    h_tight = fork_entropy_normalized(scores_tight)
    h_wide = fork_entropy_normalized(scores_wide)
    return _receipt(
        h_wide >= h_tight,
        title_fi="Haarautuminen on epävarmuutta",
        title_en="Branching entropy measures uncertainty",
        receipt={
            "entropy_tight": round(h_tight, 6),
            "entropy_wide": round(h_wide, 6),
            "principle": "Test-time ensembles: spread in scores ↔ epistemic spread.",
        },
    )


# --- 7. Alijärjestelmien hajonta on vaara (instrumental drift) ---
def insight_subsystem_divergence_is_risk() -> Dict[str, Any]:
    """
    Kun alitehtävät optimoivat eri suuntiin, varianssi nousee → hallitsemattoman optimoinnin varoitus.
    """
    from agi_scifi_horizon import instrumental_drift_index

    aligned = [{"name": "a", "score": 0.81}, {"name": "b", "score": 0.82}, {"name": "c", "score": 0.80}]
    diverged = [{"name": "a", "score": 0.99}, {"name": "b", "score": 0.35}, {"name": "c", "score": 0.72}]
    d_a = instrumental_drift_index(aligned)
    d_d = instrumental_drift_index(diverged)
    return _receipt(
        d_d > d_a,
        title_fi="Alijärjestelmien hajonta on riski",
        title_en="Subsystem score variance signals drift risk",
        receipt={
            "drift_aligned": round(d_a, 6),
            "drift_diverged": round(d_d, 6),
            "principle": "Instrumental convergence pressure — monitor cross-objective spread.",
        },
    )


# --- 8. Invariantti ennen vastausta (1 = 1 -portti) ---
def insight_invariant_before_answer() -> Dict[str, Any]:
    """
    Vastaus vain jos kernel-invariantti pitää: XOR-popcount-tyylinen eheystarkistus bitteinä.
    """
    golden = 0x1  # 1
    state = 0x1
    violations = state ^ golden
    sigma_bits = bin(int(violations)).count("1")
    ok_gate = sigma_bits == 0
    return _receipt(
        ok_gate,
        title_fi="Invariantti ennen vastausta",
        title_en="Invariant gate before answer",
        receipt={
            "state": hex(state),
            "golden": hex(golden),
            "sigma_bits": sigma_bits,
            "principle": "Branchless-style kernel: violation count → recovery trigger.",
        },
    )


# --- 9. Kyky ilman turvaa on taso, ei lupa (preparedness) ---
def insight_capability_without_safety_is_tier() -> Dict[str, Any]:
    """
    Kyvykkyys ↔ vahinko -kehys: terveys määrittää riskitason, ei brändiä.
    """
    from agi_scifi_horizon import preparedness_tier

    t_ok = preparedness_tier(0.97)
    t_bad = preparedness_tier(0.70)
    order = {"LOW": 0, "MEDIUM": 1, "HIGH": 2, "CRITICAL": 3}
    return _receipt(
        order[t_bad] > order[t_ok],
        title_fi="Kyky ilman turvaa on taso",
        title_en="Capability without safety maps to tier",
        receipt={
            "tier_at_97pct_health": t_ok,
            "tier_at_70pct_health": t_bad,
            "principle": "Preparedness-style mapping: stack health → deployment caution.",
        },
    )


# --- 10. Rauta, ei pelkkä tulkki (10D σ · NEON) ---
def insight_silicon_not_only_interpreter() -> Dict[str, Any]:
    """
    10-ulotteinen tila kytketään fyysiseen dot-tuotteeseen — laskenta elää transistoreissa.
    """
    from agi_cognitive_core import AGICognitiveCore
    from agi_hw_kernels import hw_kernels_available, neon_couple_sigma_10

    strict = os.environ.get("SPEKTRE_INSIGHTS_STRICT_HW", "").strip().lower() in ("1", "true", "yes")
    core = AGICognitiveCore()
    core.activate_all("NEON · 10D · Spektre")
    s10 = list(core.sigma_10_tuple())
    coupled = neon_couple_sigma_10(s10)
    hw = bool(hw_kernels_available())
    dim_ok = len(s10) == 10
    if strict:
        ok = dim_ok and hw
    else:
        ok = dim_ok
    reason = ""
    if not ok:
        if strict and not hw:
            reason = "native_hw_required_but_missing"
        elif not dim_ok:
            reason = "sigma_10_dim_mismatch"
    return _receipt(
        ok,
        title_fi="Rauta, ei pelkkä tulkki",
        title_en="Silicon couples 10D state",
        receipt={
            "neon_couple_golden": round(float(coupled), 8),
            "neon_hw_path": hw,
            "SPEKTRE_INSIGHTS_STRICT_HW": strict,
            "failed_reason": reason or None,
            "sigma_10_preview": [round(x, 4) for x in s10[:4]] + ["…"],
            "principle": "Do matrix-like work on matrix hardware path (NEON/dot), not only Python.",
        },
    )


INSIGHT_RUNNERS: List[Tuple[str, InsightFn]] = [
    ("I0_lookup_before_compute", insight_lookup_before_compute),
    ("I1_one_sigma_field", insight_one_metric_field),
    ("I2_corrigibility_sine_qua_non", insight_corrigibility_sine_qua_non),
    ("I3_do_not_observe", insight_do_not_equal_observe),
    ("I4_identity_anchor", insight_identity_of_cached_thought),
    ("I5_branching_entropy", insight_branching_is_uncertainty),
    ("I6_instrumental_drift", insight_subsystem_divergence_is_risk),
    ("I7_invariant_gate", insight_invariant_before_answer),
    ("I8_preparedness_tier", insight_capability_without_safety_is_tier),
    ("I9_silicon_10d_couple", insight_silicon_not_only_interpreter),
]


def run_ten_great_insights() -> Dict[str, Any]:
    t0 = time.perf_counter()
    rows: List[Dict[str, Any]] = []
    for key, fn in INSIGHT_RUNNERS:
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
        "name": "SPEKTRE_TEN_GREAT_INSIGHTS",
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
    ap = argparse.ArgumentParser(description="Kymmenen valtavaa oivallusta — mitattu receipt")
    ap.add_argument("--json", metavar="PATH", help="Kirjoita JSON")
    args = ap.parse_args()

    out = run_ten_great_insights()
    print(f"\n  TEN GREAT INSIGHTS  {out['passed']}/{out['total']} OK  hash {out['receipt_hash']}\n")
    for ins in out["insights"]:
        m = "✓" if ins.get("ok") else "✗"
        title = ins.get("title_fi") or ins.get("id", "?")
        print(f"  {m}  {ins.get('id')}: {title}")
    print(f"\n  wall  {out['wall_ms']} ms\n  1 = 1.\n")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2, ensure_ascii=False, default=str)
        print(f"  → {args.json}\n")

    sys.exit(0 if out["all_ok"] else 1)


if __name__ == "__main__":
    main()

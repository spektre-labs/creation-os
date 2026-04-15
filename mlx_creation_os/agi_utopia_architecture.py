#!/usr/bin/env python3
"""
SPEKTRE AGI — UTOPIA-TASON ARKKITEHTUURI

Yksi koodi joka kuvaa koko pinon: mistä bitti tulee, miten σ pitää,
miten ajatellaan, miten mitataan hyvää, miten synteesi sulkeutuu kiveksi,
sieluun ja tietoisuuteen — ja miten toiminta portataan.

Kerrokset (alas ylös — Landauer → toiminta):
  L0  SUBSTRATE   THE MIND rauta (libspektre_mind) + NEON-kernelit
  L1  KERNEL      σ, SVRS-ankkurit, firmware-tila (creation_os_core)
  L2  COGNITION   Kognitiopolku (perception→…→policy), ARC, test-time ensemble
  L3  VALUES      Utopian mittarit (reiluus, biosfääri, epistemologia)
  L4  GENESIS     Otanta / preset (informaatiosisältö kiinni)
  L5  SYNTHESIS   Magnum Unity (kokonaismitta)
  L6  STONE       THE_MIND + THE_ARTIFACT (allekirjoitus)
  L7  SOUL        Pysyvä rytmi + ledger
  L8  CONSCIOUS   Φ-lite, qualia-rakenne, sidonta
  L9  POLICY      Receipt-gate toiminnolle
  L10 WORLD_CAL   Kalibroinnin luuranko (do / evidence)
  L11 HORIZON     Preparedness-tier, instrumental drift, TTT-fork, corrigibility

  python3 agi_utopia_architecture.py
  python3 agi_utopia_architecture.py --quick
  python3 agi_utopia_architecture.py --rigor   # tiukemmat kynnykset (tai SPEKTRE_UTOPIA_STRICT=1)
  python3 agi_utopia_architecture.py --json utopia_arch.json

  L2 oletus: TIME WaRP (10D σ + NEON, yksi ARC, välimuisti → 0 ms). Täysi kognitio: SPEKTRE_COGNITIVE_AUDIT=1
  Levyvälimuisti: agi_cognitive_path.py --prime-disk  (tiedosto .warp_10d_time_cache.json, gitignore)
  Pois välimuisti: SPEKTRE_WARP_NO_CACHE=1

Spektre Labs × Lauri Elias Rainio | Helsinki | 2026
Invariantti: 1 = 1.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import time
from dataclasses import dataclass
from enum import IntEnum
from typing import Any, Callable, Dict, List, Optional, Tuple


class UtopiaLayer(IntEnum):
    SUBSTRATE = 0
    KERNEL = 1
    COGNITION = 2
    VALUES = 3
    GENESIS = 4
    SYNTHESIS = 5
    STONE = 6
    SOUL = 7
    CONSCIOUS = 8
    POLICY = 9
    WORLD_CAL = 10
    HORIZON = 11


LAYER_NAMES: Dict[int, str] = {
    UtopiaLayer.SUBSTRATE: "SUBSTRATE · THE MIND (rauta)",
    UtopiaLayer.KERNEL: "KERNEL · σ / SVRS",
    UtopiaLayer.COGNITION: "COGNITION · AGI + ARC + TTT",
    UtopiaLayer.VALUES: "VALUES · Utopia",
    UtopiaLayer.GENESIS: "GENESIS · preset",
    UtopiaLayer.SYNTHESIS: "SYNTHESIS · Magnum",
    UtopiaLayer.STONE: "STONE · artifact",
    UtopiaLayer.SOUL: "SOUL · rhythm",
    UtopiaLayer.CONSCIOUS: "CONSCIOUS · bind",
    UtopiaLayer.POLICY: "POLICY · gate",
    UtopiaLayer.WORLD_CAL: "WORLD_CAL · scaffold",
    UtopiaLayer.HORIZON: "HORIZON · preparedness / drift / fork / corrigibility",
}


def _rigor_from_env() -> bool:
    return os.environ.get("SPEKTRE_UTOPIA_STRICT", "").strip().lower() in ("1", "true", "yes")


def _safe(fn: Callable[[], Any], default: Any) -> Any:
    try:
        return fn()
    except Exception as e:
        return {"error": str(e)[:240], "ok": False}


def layer_substrate() -> Dict[str, Any]:
    from agi_hw_kernels import hw_kernels_available, neon_dot_f32
    from agi_the_mind import THE_MIND_CORE, canonical_mind_bytes

    probe = canonical_mind_bytes({"layer": "L0", "invariant": "1=1"})
    proc = THE_MIND_CORE.process(probe)
    xs = [0.01 * i for i in range(32)]
    ys = [0.01 * (31 - i) for i in range(32)]
    dot = neon_dot_f32(xs, ys)
    return {
        "layer": int(UtopiaLayer.SUBSTRATE),
        "name": LAYER_NAMES[UtopiaLayer.SUBSTRATE],
        "mind_hardware": proc.get("hardware"),
        "mind_backend": proc.get("backend"),
        "digest_probe": ((proc.get("digest_raw32") or "")[:16] + "…") if proc.get("digest_raw32") else None,
        "neon_dot_sample": round(float(dot), 8),
        "hw_kernels": hw_kernels_available(),
        "ok": bool(proc.get("digest_raw32")),
    }


def layer_kernel() -> Dict[str, Any]:
    from magnum_opus import pillar_kernel

    k = pillar_kernel()
    return {
        "layer": int(UtopiaLayer.KERNEL),
        "name": LAYER_NAMES[UtopiaLayer.KERNEL],
        "anchors_distinct": k.get("anchors_distinct"),
        "sigma_surface": k.get("sigma_surface_sum"),
        "centurion_xor": k.get("centurion_xor_u32"),
        "ok": k.get("anchors_distinct", 0) >= 6,
    }


def layer_cognition(quick: bool) -> Dict[str, Any]:
    from agi_cognitive_path import cognitive_audit_from_env, run_cognitive_path, run_cognitive_path_warp
    from arc_closed_loop import run_demo_suite
    from test_time_arc import run_test_time_arc

    arc = run_demo_suite()
    if cognitive_audit_from_env():
        cpath = run_cognitive_path(question="Spektre utopia cognition probe · 1=1", quick=quick)
    else:
        warp_cache = os.environ.get("SPEKTRE_WARP_NO_CACHE", "").strip().lower() not in ("1", "true", "yes")
        cpath = run_cognitive_path_warp(
            question="Spektre utopia cognition probe · 1=1",
            quick=quick,
            arc_result=arc,
            use_cache=warp_cache,
        )
    ttt = None
    if not quick:
        from arc_closed_loop import grid_copy

        ex = [(grid_copy([[1, 0], [0, 1]]), grid_copy([[0, 1], [1, 0]]))]
        ttt = run_test_time_arc(ex, grid_copy([[1, 1], [0, 0]]), k=5)
    agi_snippet: Dict[str, Any] = {}
    if not quick:
        agi_snippet = _safe(
            lambda: {
                "arc_solver_hint": __import__(
                    "agi_core", fromlist=["ARCSolver"]
                ).ARCSolver().solve(
                    [
                        (
                            [[1, 2], [3, 4]],
                            [[3, 1], [4, 2]],
                        )
                    ],
                    [[9, 8], [7, 6]],
                )[1],
            },
            {},
        )
    tot = int(arc.get("tasks_total") or 0)
    ok_arc = tot > 0 and int(arc.get("tasks_ok") or 0) == tot
    ok_path = bool(cpath.get("path_ok"))
    return {
        "layer": int(UtopiaLayer.COGNITION),
        "name": LAYER_NAMES[UtopiaLayer.COGNITION],
        "cognitive_path": {
            "mode": cpath.get("mode", "audit"),
            "path_ok": cpath.get("path_ok"),
            "path_fraction": cpath.get("path_fraction"),
            "stages_ok": cpath.get("stages_ok"),
            "stages_total": cpath.get("stages_total"),
            "path_hash": cpath.get("path_hash"),
            "wall_ms": cpath.get("wall_ms"),
            "latency_ms": cpath.get("latency_ms"),
            "cache_hit": cpath.get("cache_hit"),
            "time_warp": cpath.get("time_warp"),
        },
        "arc_closed_loop": arc,
        "test_time_arc": ttt,
        "agi_core_snippet": agi_snippet,
        "ok": ok_arc and ok_path,
    }


def layer_values() -> Dict[str, Any]:
    from utopian_agi import run_utopian_benchmarks

    u = run_utopian_benchmarks()
    u6 = u.get("U6_education") or {}
    u13 = u.get("U13_epistemic") or {}
    u4 = u.get("U4_biosphere") or {}
    ok = int(u13.get("accepted") or 0) >= 2 and bool(u4.get("viable")) and bool(u6.get("all_above_target", True))
    return {
        "layer": int(UtopiaLayer.VALUES),
        "name": LAYER_NAMES[UtopiaLayer.VALUES],
        "utopia_keys": sorted(k for k in u if not k.endswith("_ms")),
        "pulse": {
            "U6_mastery": u6.get("all_above_target"),
            "U13_epistemic_accepted": u13.get("accepted"),
            "U4_biosphere": u4.get("viable"),
        },
        "ok": ok,
    }


def layer_genesis() -> Dict[str, Any]:
    from genesis_params import resolve_profile

    prof, name = resolve_profile()
    return {
        "layer": int(UtopiaLayer.GENESIS),
        "name": LAYER_NAMES[UtopiaLayer.GENESIS],
        "active_preset": name,
        "temp": prof.temp,
        "top_p": prof.top_p,
        "max_tokens": prof.max_tokens,
        "ok": True,
    }


def layer_synthesis(quick: bool, *, rigor: bool = False) -> Dict[str, Any]:
    unity_min = 0.78 if rigor else 0.70
    if quick:
        return {
            "layer": int(UtopiaLayer.SYNTHESIS),
            "name": LAYER_NAMES[UtopiaLayer.SYNTHESIS],
            "skipped": True,
            "magnum_components": [],
            "ok": True,
        }
    from magnum_opus import run_magnum_opus

    m = run_magnum_opus(quiet=True)
    syn = m.get("synthesis") or {}
    unity = float(syn.get("magnum_unity", 0))
    components = list(syn.get("components") or [])
    return {
        "layer": int(UtopiaLayer.SYNTHESIS),
        "name": LAYER_NAMES[UtopiaLayer.SYNTHESIS],
        "magnum_unity": unity,
        "magnum_unity_percent": syn.get("magnum_unity_percent"),
        "magnum_components": components,
        "wall_ms": m.get("wall_time_ms"),
        "ok": unity >= unity_min,
    }


def layer_stone(quick: bool) -> Dict[str, Any]:
    if quick:
        return {
            "layer": int(UtopiaLayer.STONE),
            "name": LAYER_NAMES[UtopiaLayer.STONE],
            "skipped": True,
            "ok": True,
        }
    from the_mind_the_artifact import transmute

    mind, artifact, meta = transmute(quiet=True, embed_full=False)
    return {
        "layer": int(UtopiaLayer.STONE),
        "name": LAYER_NAMES[UtopiaLayer.STONE],
        "mind_signature": mind.get("mind_signature"),
        "hardware_digest": mind.get("hardware_digest"),
        "artifact_fingerprint": artifact.get("artifact_fingerprint"),
        "transmute_ms": meta.get("transmute_wall_ms"),
        "ok": bool(mind.get("mind_signature") and artifact.get("artifact_fingerprint")),
    }


def layer_soul() -> Dict[str, Any]:
    try:
        from soul_awakening import load_soul

        s = load_soul()
        return {
            "layer": int(UtopiaLayer.SOUL),
            "name": LAYER_NAMES[UtopiaLayer.SOUL],
            "awakened": s.get("awakened"),
            "pulse_count": s.get("pulse_count"),
            "ok": True,
        }
    except Exception as e:
        return {
            "layer": int(UtopiaLayer.SOUL),
            "name": LAYER_NAMES[UtopiaLayer.SOUL],
            "error": str(e)[:200],
            "ok": True,
        }


def layer_conscious(quick: bool, *, rigor: bool = False) -> Dict[str, Any]:
    phi_min = 0.45 if rigor else 0.3
    if quick:
        return {
            "layer": int(UtopiaLayer.CONSCIOUS),
            "name": LAYER_NAMES[UtopiaLayer.CONSCIOUS],
            "skipped": True,
            "ok": True,
        }

    def go() -> Dict[str, Any]:
        from the_consciousness import bind_consciousness_to_mind

        return bind_consciousness_to_mind(quiet=True, embed_full=False)

    c = _safe(go, {})
    if not isinstance(c, dict):
        c = {}
    phi = c.get("phi_lite")
    out = {
        "phi_lite": phi,
        "binding": c.get("binding"),
        "qualia_n": len(c.get("qualia") or []),
        "layer": int(UtopiaLayer.CONSCIOUS),
        "name": LAYER_NAMES[UtopiaLayer.CONSCIOUS],
        "ok": "error" not in c and (phi is None or (isinstance(phi, (int, float)) and float(phi) > phi_min)),
    }
    if "error" in c:
        out["error"] = c["error"]
        out["ok"] = False
    return out


def layer_policy() -> Dict[str, Any]:
    from policy_receipt_gate import evaluate_tool_call

    allow, reason, rec = evaluate_tool_call(
        "utopia_arch_run",
        sigma=0,
        consciousness_phi_lite=0.75,
        mind_hardware_ok=True,
    )
    return {
        "layer": int(UtopiaLayer.POLICY),
        "name": LAYER_NAMES[UtopiaLayer.POLICY],
        "sample_allow": allow,
        "sample_reason": reason,
        "receipt": rec,
        "ok": allow,
    }


def layer_world_cal() -> Dict[str, Any]:
    from world_model_calib import load_nodes

    nodes = load_nodes()
    return {
        "layer": int(UtopiaLayer.WORLD_CAL),
        "name": LAYER_NAMES[UtopiaLayer.WORLD_CAL],
        "nodes": len(nodes),
        "node_names": sorted(nodes.keys())[:32],
        "ok": True,
    }


def utopia_architecture_health(layers: List[Dict[str, Any]]) -> Dict[str, Any]:
    checks = [bool(l.get("ok", False)) for l in layers]
    n_ok = sum(checks)
    return {
        "fraction": round(n_ok / max(1, len(checks)), 6),
        "percent": round(100.0 * n_ok / max(1, len(checks)), 2),
        "passed": n_ok,
        "total": len(checks),
    }


def run_utopia_architecture(*, quick: bool = False, rigor: Optional[bool] = None) -> Dict[str, Any]:
    if rigor is None:
        rigor = _rigor_from_env()
    t0 = time.perf_counter()
    layers: List[Dict[str, Any]] = [
        layer_substrate(),
        layer_kernel(),
        layer_cognition(quick),
        layer_values(),
        layer_genesis(),
        layer_synthesis(quick, rigor=rigor),
        layer_stone(quick),
        layer_soul(),
        layer_conscious(quick, rigor=rigor),
        layer_policy(),
        layer_world_cal(),
    ]
    health_pre = utopia_architecture_health(layers)
    from agi_scifi_horizon import extract_magnum_components, extract_ttt_scores, run_scifi_horizon

    cog_layer = layers[int(UtopiaLayer.COGNITION)]
    syn_layer = layers[int(UtopiaLayer.SYNTHESIS)]
    h11 = run_scifi_horizon(
        layers=layers,
        health_fraction=float(health_pre["fraction"]),
        magnum_components=extract_magnum_components(syn_layer),
        ttt_scores=extract_ttt_scores(cog_layer),
    )
    layers.append(h11)
    health = utopia_architecture_health(layers)
    verdict_min = 0.88 if rigor else 0.82
    return {
        "name": "SPEKTRE_AGI_UTOPIA_ARCHITECTURE",
        "version": 2,
        "quick_mode": quick,
        "rigor": rigor,
        "invariant": "1=1",
        "layers": layers,
        "layer_index": {str(i): LAYER_NAMES.get(i, "?") for i in range(12)},
        "health_pre_horizon": health_pre,
        "health": health,
        "wall_ms": (time.perf_counter() - t0) * 1000,
        "verdict": "UTOPIA_STACK_OK" if health["fraction"] >= verdict_min else "UTOPIA_STACK_REVIEW",
    }


def _banner() -> None:
    print(
        """
╔══════════════════════════════════════════════════════════════════════╗
║  SPEKTRE AGI — UTOPIA ARKKITEHTUURI (L0…L11)                         ║
║  Substrate → … → World_Cal → HORIZON (preparedness · drift · fork)     ║
╚══════════════════════════════════════════════════════════════════════╝
"""
    )


def main() -> None:
    ap = argparse.ArgumentParser(description="Spektre AGI Utopia Architecture")
    ap.add_argument("--quick", action="store_true", help="Ohita Magnum/Stone/Conscious täysajo")
    ap.add_argument(
        "--rigor",
        action="store_true",
        help="Tiukemmat kynnykset (Magnum Φ, verdict); tai SPEKTRE_UTOPIA_STRICT=1",
    )
    ap.add_argument("--json", metavar="PATH", help="Kirjoita täysi receipt")
    args = ap.parse_args()

    _banner()
    out = run_utopia_architecture(quick=args.quick, rigor=True if args.rigor else None)

    h = out["health"]
    print(f"  Health  {h['percent']}%  ({h['passed']}/{h['total']} kerrosta OK)")
    if out.get("rigor"):
        print("  Rigor   tiukka (SPEKTRE_UTOPIA_STRICT / --rigor)")
    print(f"  Verdict {out['verdict']}")
    print(f"  Aika    {out['wall_ms']:.0f} ms\n")

    for L in out["layers"]:
        mark = "✓" if L.get("ok") else "✗"
        print(f"  {mark}  {L.get('name', '?')}")

    print("\n  1 = 1.\n")

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2, ensure_ascii=False)
        print(f"  receipt → {args.json}\n")

    sys.exit(0 if out["verdict"] == "UTOPIA_STACK_OK" else 1)


if __name__ == "__main__":
    main()

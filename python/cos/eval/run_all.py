# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""One-shot **host eval bundle**: pytest, coverage, σ micro-bench, CLI import path, σ pairs, wheel
build probe, Python module count, C/H line count.

**Not** a certified benchmark harness — wall time and pass rates are machine-specific. Always archive
host metadata for any off-repo claims (``docs/REPRO_BUNDLE_TEMPLATE.md``). **Not AGI achieved.**

Optional ``--quick``: small pytest slice, ``cos.sigma_gate`` coverage only, **skips** ``python -m build``
(~seconds on a warm laptop). **Full** ``cos eval-all`` runs ``tests/`` + ``--cov=cos`` and can take
tens of minutes — not merge-gate parity by itself.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

__all__ = ["main", "repo_root", "run_cmd"]

_JSON_OUT_MAX = 500_000


def repo_root() -> Path:
    env = (os.environ.get("CREATION_OS_ROOT") or "").strip()
    if env:
        return Path(env).expanduser().resolve()
    here = Path(__file__).resolve()
    for base in (Path.cwd().resolve(), *here.parents):
        if (base / "creation_os_v2.c").is_file():
            return base
    return here.parents[4]


def run_cmd(
    argv: List[str],
    *,
    cwd: Path,
    env: Dict[str, str],
    timeout: Optional[float] = None,
) -> Dict[str, Any]:
    """Run *argv* (no shell). Return structured result; never swallow stderr."""
    try:
        r = subprocess.run(
            argv,
            cwd=str(cwd),
            env=env,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
        return {
            "ok": r.returncode == 0,
            "code": r.returncode,
            "out": r.stdout or "",
            "err": r.stderr or "",
        }
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") if isinstance(e.stdout, str) else ""
        err = (e.stderr or "") if isinstance(e.stderr, str) else "TIMEOUT"
        return {"ok": False, "code": -1, "out": out, "err": err or "TIMEOUT"}
    except Exception as e:
        return {"ok": False, "code": -1, "out": "", "err": str(e)}


def _truncate(s: str, limit: int = _JSON_OUT_MAX) -> tuple[str, bool]:
    if len(s) <= limit:
        return s, False
    return s[:limit] + "\n… [truncated for JSON size]\n", True


def _quality_script() -> str:
    return """
from cos.eval.gemma_eval import GPQA_SAMPLE
from cos.sigma_gate import SigmaGate

gate = SigmaGate()
rows = GPQA_SAMPLE[:8]
ok_n = 0
for row in rows:
    q = row["q"]
    c = row["correct"]
    w = row["wrong"]
    s_c, _ = gate.score(q, c)
    s_w, _ = gate.score(q, w)
    ok = float(s_w) > float(s_c)
    ok_n += int(ok)
    print(
        "  σ(correct)=%.3f σ(wrong)=%.3f %s | %s"
        % (float(s_c), float(s_w), "OK" if ok else "XX", q[:52])
    )
pct = 100.0 * ok_n / len(rows)
print(
    "Reference discrimination σ(wrong)>σ(correct): %d/%d (%.0f%%) "
    "(synthetic probes; not GPQA Diamond)"
    % (ok_n, len(rows), pct)
)
""".strip()


def _sigma_speed_script() -> str:
    return """
import time
from cos.sigma_gate import SigmaGate
gate = SigmaGate()
n = 1000
start = time.perf_counter()
for _i in range(n):
    gate.score("test prompt", "test response")
elapsed = time.perf_counter() - start
per_ms = elapsed / n * 1000.0
cps = n / elapsed if elapsed > 0 else 0.0
print(f"{per_ms:.3f}ms per call ({cps:.0f} calls/sec)")
""".strip()


def _cli_startup_script() -> str:
    return """
import time
start = time.perf_counter()
from cos.sigma_gate import SigmaGate
gate = SigmaGate()
_, _ = gate.score("test", "test")
elapsed = (time.perf_counter() - start) * 1000.0
print(f"{elapsed:.0f}ms startup+score")
""".strip()


def _count_c_lines(root: Path) -> tuple[int, int]:
    """Return (files, total_lines) for tracked ``*.c`` / ``*.h`` (``git ls-files``), excluding ``third_party``."""
    r = subprocess.run(
        ["git", "-C", str(root), "ls-files"],
        capture_output=True,
        text=True,
        check=False,
    )
    if r.returncode != 0:
        return 0, 0
    n_files = 0
    n_lines = 0
    for rel in (ln.strip() for ln in r.stdout.splitlines() if ln.strip()):
        if not (rel.endswith(".c") or rel.endswith(".h")):
            continue
        if "third_party" in rel.split("/"):
            continue
        p = root / rel
        if not p.is_file():
            continue
        try:
            txt = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        n_files += 1
        n_lines += txt.count("\n") + (1 if txt and not txt.endswith("\n") else 0)
    return n_files, n_lines


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Run Creation OS host eval bundle (pytest, coverage, σ bench, …).",
    )
    ap.add_argument(
        "--quick",
        action="store_true",
        help="Run a small pytest subset (~15s target on a warm laptop); not merge-gate parity.",
    )
    ap.add_argument(
        "--pytest-timeout",
        type=float,
        default=7200.0,
        metavar="SEC",
        help="Wall-clock limit for full pytest (default 7200s). Quick mode uses 600s.",
    )
    ap.add_argument(
        "--out",
        type=str,
        default="",
        help="JSON path (default: <repo>/eval_results/full_eval.json)",
    )
    ap.add_argument("--json-summary", action="store_true", help="Print one JSON object to stdout only")
    args = ap.parse_args(argv)

    root = repo_root()
    py = sys.executable
    env = {**os.environ, "PYTHONPATH": str(root / "python"), "PYTHONWARNINGS": "ignore"}

    out_path = Path(args.out) if args.out else root / "eval_results" / "full_eval.json"

    results: Dict[str, Any] = {}
    t0 = time.perf_counter()

    def _emit(msg: str) -> None:
        if not args.json_summary:
            print(msg, flush=True)

    if not args.json_summary:
        print("=" * 60, flush=True)
        print("CREATION OS — FULL EVALUATION (host bundle)", flush=True)
        print("=" * 60, flush=True)

    # 1 pytest
    _emit("\n[1/8] pytest...")
    quick_tests = [
        "tests/test_sigma_properties.py",
        "tests/test_gate.py",
        "tests/test_probes.py",
        "tests/test_discrimination.py",
        "tests/test_hash_embedding.py",
        "tests/test_health.py",
        "tests/test_agi_demo.py",
        "tests/test_edge.py",
        "tests/test_sigma_gate_core.py",
        "tests/test_gemma_eval_lab.py",
    ]
    if args.quick:
        pt_argv = [py, "-m", "pytest", "-q", "--tb=no", *quick_tests]
        pt_to = min(600.0, float(args.pytest_timeout))
    else:
        pt_argv = [py, "-m", "pytest", "tests/", "-q", "--tb=no"]
        pt_to = float(args.pytest_timeout)
    r1 = run_cmd(pt_argv, cwd=root, env=env, timeout=pt_to)
    lines = r1["out"].strip().split("\n")
    last = lines[-1] if lines else ""
    results["pytest"] = {
        "pass": r1["ok"],
        "code": r1["code"],
        "summary_line": last,
        "stdout_full": _truncate(r1["out"])[0],
        "stdout_truncated": _truncate(r1["out"])[1],
        "stderr": r1["err"],
    }
    _emit(f"  {last}")
    if r1["err"]:
        _emit(f"  stderr:\n{r1['err']}")

    # 2 coverage
    _emit("\n[2/8] coverage (pytest-cov)...")
    if args.quick:
        cov_targets = ["--cov=cos.sigma_gate"]
        cov_note = "quick: cos.sigma_gate only (full run uses --cov=cos)"
    else:
        cov_targets = ["--cov=cos"]
        cov_note = "full python/cos tree"
    cov_argv = [
        py,
        "-m",
        "pytest",
        *(quick_tests if args.quick else ["tests/"]),
        *cov_targets,
        "--cov-branch",
        "--cov-report=term-missing:skip-covered",
        "-q",
        "--tb=no",
    ]
    r2 = run_cmd(cov_argv, cwd=root, env=env, timeout=pt_to)
    cov_tail = "\n".join(r2["out"].strip().split("\n")[-12:])
    results["coverage"] = {
        "pass": r2["ok"],
        "code": r2["code"],
        "tail": cov_tail,
        "note": cov_note,
        "stdout_full": _truncate(r2["out"])[0],
        "stdout_truncated": _truncate(r2["out"])[1],
        "stderr": r2["err"],
    }
    _emit(f"  {cov_tail[:500]}")
    if r2["err"]:
        _emit(f"  stderr:\n{r2['err'][:2000]}")

    # 3 sigma speed
    _emit("\n[3/8] σ-gate speed...")
    r3 = run_cmd(
        [py, "-c", _sigma_speed_script()],
        cwd=root,
        env=env,
        timeout=120.0,
    )
    results["sigma_speed"] = {
        "pass": r3["ok"],
        "line": r3["out"].strip(),
        "stderr": r3["err"],
    }
    _emit(f"  {r3['out'].strip()}")
    if r3["err"]:
        _emit(f"  stderr: {r3['err']}")

    # 4 CLI startup
    _emit("\n[4/8] CLI import + score (cold-ish)...")
    r4 = run_cmd(
        [py, "-c", _cli_startup_script()],
        cwd=root,
        env=env,
        timeout=120.0,
    )
    results["cli_speed"] = {"pass": r4["ok"], "line": r4["out"].strip(), "stderr": r4["err"]}
    _emit(f"  {r4['out'].strip()}")

    # 5 quality
    _emit("\n[5/8] σ-gate quality (known pairs)...")
    r5 = run_cmd(
        [py, "-c", _quality_script()],
        cwd=root,
        env=env,
        timeout=120.0,
    )
    results["quality"] = {
        "pass": r5["ok"],
        "stdout": r5["out"],
        "stderr": r5["err"],
    }
    _emit(r5["out"])
    if r5["err"]:
        _emit(r5["err"])

    # 6 build
    _emit("\n[6/8] Python sdist/wheel (python -m build)...")
    if args.quick:
        btail = "(skipped in --quick — run full ``cos eval-all`` for wheel/sdist probe)"
        results["build"] = {
            "pass": True,
            "skipped": True,
            "tail": btail,
            "stdout_full": "",
            "stderr": "",
        }
        _emit(f"  {btail}")
    else:
        r6 = run_cmd([py, "-m", "build"], cwd=root, env=env, timeout=600.0)
        btail = "\n".join(r6["out"].strip().split("\n")[-8:])
        results["build"] = {
            "pass": r6["ok"],
            "skipped": False,
            "tail": btail,
            "stdout_full": _truncate(r6["out"])[0],
            "stderr": r6["err"],
        }
        _emit(f"  {btail[:500]}")
        if not r6["ok"]:
            _emit(f"  build stderr:\n{r6['err'][:4000]}")

    # 7 module count
    _emit("\n[7/8] module count (python/cos)...")
    cos_pkg = root / "python" / "cos"
    py_files = [p for p in cos_pkg.rglob("*.py") if "__pycache__" not in p.parts]
    n_mod = len(py_files)
    results["modules"] = {"pass": True, "count": n_mod, "note": "all *.py under python/cos"}
    _emit(f"  {n_mod} Python files under python/cos")

    # 8 C lines
    _emit("\n[8/8] C/H line count (repo minus third_party)…")
    cf, cl = _count_c_lines(root)
    results["c_lines"] = {"pass": True, "files": cf, "lines": cl}
    _emit(f"  {cf} files, {cl} lines")

    elapsed = time.perf_counter() - t0
    checks = [
        results["pytest"]["pass"],
        results["coverage"]["pass"],
        results["sigma_speed"]["pass"],
        results["cli_speed"]["pass"],
        results["quality"]["pass"],
        results["build"]["pass"],
        results["modules"]["pass"],
        results["c_lines"]["pass"],
    ]
    passed = sum(1 for x in checks if x)
    results["_wall_seconds"] = round(elapsed, 3)
    results["_checks_passed"] = passed
    results["_checks_total"] = len(checks)
    results["_disclaimer"] = (
        "Host-only; not merge-gate. Do not merge microbench lines with harness MMLU/ARC. "
        "NOT AGI ACHIEVED."
    )

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        json.dump(results, f, indent=2, default=str)

    if args.json_summary:
        print(
            json.dumps(
                {
                    "saved_to": str(out_path),
                    "wall_seconds": elapsed,
                    "checks_passed": passed,
                    "checks_total": len(checks),
                    "pytest_summary": results["pytest"].get("summary_line"),
                    "disclaimer": results["_disclaimer"],
                },
                indent=2,
                default=str,
            )
        )
        return 0 if passed == len(checks) else 1

    print("\n" + "=" * 60, flush=True)
    print("SUMMARY", flush=True)
    print("=" * 60, flush=True)
    bdg = results["build"]
    if bdg.get("skipped"):
        build_line = "SKIP (--quick)"
    else:
        build_line = "OK" if bdg["pass"] else "FAIL (see stderr in JSON)"
    print(
        f"""
pytest:      {results['pytest']['summary_line']}
coverage:    (see tail in JSON) {results['coverage']['tail'][:120].replace(chr(10), ' ')}
σ-speed:     {results['sigma_speed']['line']}
CLI:         {results['cli_speed']['line']}
build:       {build_line}
modules:     {n_mod} Python files under python/cos
C/H:         {cf} files, {cl} lines (third_party excluded)

Checks:      {passed}/{len(checks)} subprocess steps returned ok
Time:        {elapsed:.1f}s
Saved:       {out_path}

NOT AGI ACHIEVED. σ. 1=1.
""".rstrip()
    )
    return 0 if passed == len(checks) else 1


if __name__ == "__main__":
    raise SystemExit(main())

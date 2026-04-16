#!/usr/bin/env python3
"""
HYPERVEC swarm — multiprocessing agents fold SHA-derived vectors (stdlib only).

Orbital references (see README): MoonBit (WASM-native toolchain), Bend/HVM2 (GPU-parallel IR).
This file intentionally avoids numpy/ray/torch — speed comes from CPython + process (or thread) parallelism on shards. Python 3.9+.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import multiprocessing as mp
import os
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor, ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Iterable, Sequence


def _digest_to_vec(digest: bytes, dims: int) -> list[float]:
    """Expand 32-byte digest into `dims` floats in [-1, 1] by cycling XOR-fold."""
    if dims <= 0:
        return []
    out: list[float] = []
    buf = bytearray(digest)
    i = 0
    while len(out) < dims:
        if i + 4 > len(buf):
            buf.extend(hashlib.sha256(bytes(buf)).digest())
        chunk = int.from_bytes(buf[i : i + 4], "little", signed=False)
        i += 4
        # map uint32 → [-1,1]
        out.append((chunk / 4294967295.0) * 2.0 - 1.0)
    return out


def _vec_add(a: Sequence[float], b: Sequence[float]) -> list[float]:
    if len(a) != len(b):
        raise ValueError("vector dim mismatch")
    return [x + y for x, y in zip(a, b)]


def _vec_l2(v: Sequence[float]) -> float:
    return math.sqrt(sum(x * x for x in v))


def _shard_ranges(size: int, agents: int) -> list[tuple[int, int]]:
    if agents <= 0 or size <= 0:
        return []
    agents = min(agents, max(1, size))
    base = size // agents
    rem = size % agents
    out: list[tuple[int, int]] = []
    pos = 0
    for i in range(agents):
        span = base + (1 if i < rem else 0)
        end = pos + span
        if span > 0:
            out.append((pos, end))
        pos = end
    return out


def _agent_job(args: tuple[str, int, int, int]) -> list[float]:
    path, start, end, dims = args
    p = Path(path)
    data = p.read_bytes()[start:end]
    h = hashlib.sha256(data).digest()
    return _digest_to_vec(h, dims)


def _load_manifest(root: Path) -> dict:
    mpath = root / "hypervec" / "manifest.json"
    return json.loads(mpath.read_text(encoding="utf-8"))


def _iter_jobs(root: Path, agents: int, dims: int) -> Iterable[tuple[str, int, int, int]]:
    manifest = _load_manifest(root)
    for rel in manifest["fragments"]:
        path = root / rel
        if not path.is_file():
            continue
        raw = path.read_bytes()
        n = len(raw)
        for start, end in _shard_ranges(n, agents):
            yield (str(path), start, end, dims)


def _make_pool(workers: int):
    """Unix: fork pool (fast, no pickle of __main__). Windows: threads (stdlib-only)."""
    if sys.platform == "win32":
        return ThreadPoolExecutor(max_workers=workers)
    try:
        ctx = mp.get_context("fork")
        return ProcessPoolExecutor(max_workers=workers, mp_context=ctx)
    except (ValueError, OSError):
        return ThreadPoolExecutor(max_workers=workers)


def run_swarm(root: Path, agents: int, dims: int) -> tuple[list[float], int]:
    jobs = list(_iter_jobs(root, agents, dims))
    if not jobs:
        raise SystemExit("hypervec: no shard jobs (missing sources or empty manifest?)")

    acc = [0.0] * dims
    workers = min(len(jobs), max(1, (os.cpu_count() or 2)))
    done = 0
    with _make_pool(workers) as ex:
        futs = [ex.submit(_agent_job, j) for j in jobs]
        for fut in as_completed(futs):
            acc = _vec_add(acc, fut.result())
            done += 1
    return acc, done


def maybe_materialize(root: Path) -> None:
    if os.environ.get("HYPERVEC_MATERIALIZE", "").lower() not in ("1", "true", "yes"):
        return
    print("HYPERVEC_MATERIALIZE: invoking make merge-gate …", file=sys.stderr)
    r = subprocess.run(["make", "merge-gate"], cwd=str(root))
    if r.returncode != 0:
        raise SystemExit(r.returncode)


def main() -> int:
    ap = argparse.ArgumentParser(description="Hypervec swarm over Creation OS sources")
    ap.add_argument("--agents", type=int, default=0, help="shard count per file (0 = manifest default)")
    ap.add_argument("--dims", type=int, default=0, help="vector dimension (0 = manifest default)")
    ap.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="creation-os root (directory containing Makefile)",
    )
    args = ap.parse_args()
    root: Path = args.root
    manifest = _load_manifest(root)
    agents = args.agents or int(manifest.get("default_agents", 16))
    dims = args.dims or int(manifest.get("default_dims", 64))

    vec, n = run_swarm(root, agents, dims)
    energy = _vec_l2(vec)
    print(f"hypervec: agents_completed={n} dims={dims} L2_energy={energy:.6f}")
    maybe_materialize(root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

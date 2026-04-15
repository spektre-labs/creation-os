"""
ARC CLOSED LOOP — hypoteesi → simuloi → pisteytä → tarkista (ilman LLM:ää).

Next-level oivallus: ohjelma on **sekvenssi** primitiivejä; suljettu silmukka on
beam-etsintä + vastaesimerkin palautus (backtrack) kun yksi esimerkki rikkoo säännön.

Viitekehys: ARC-AGI korostaa kompositiota ja kontekstia — tämä moduuli on
**falsifioitava** polku: jokainen askel on ajettava gridillä.

1 = 1.
"""
from __future__ import annotations

import hashlib
import json
import time
from copy import deepcopy
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union

Grid = List[List[int]]
Op = Union[str, Tuple[str, Any]]

# Geometriat (yhteensopiva agi_core.ARCSolver.TRANSFORMS -nimien kanssa)
_GEO: Dict[str, Callable[[Grid], Grid]] = {
    "identity": lambda g: [row[:] for row in g],
    "rot90": lambda g: [list(r) for r in zip(*g[::-1])],
    "rot180": lambda g: [row[::-1] for row in g[::-1]],
    "rot270": lambda g: [list(r) for r in zip(*g)][::-1],
    "flipH": lambda g: [row[::-1] for row in g],
    "flipV": lambda g: g[::-1],
    "transpose": lambda g: [list(r) for r in zip(*g)],
}


def grid_copy(g: Grid) -> Grid:
    return [row[:] for row in g]


def grid_dims(g: Grid) -> Tuple[int, int]:
    return (len(g), len(g[0]) if g else 0)


def apply_geo(name: str, g: Grid) -> Grid:
    fn = _GEO.get(name)
    if fn is None:
        raise ValueError(f"unknown geo {name}")
    return fn(g)


def apply_color_map(g: Grid, cmap: Dict[int, int]) -> Grid:
    return [[cmap.get(c, c) for c in row] for row in g]


def apply_replace_all(g: Grid, src: int, dst: int) -> Grid:
    return [[dst if c == src else c for c in row] for row in g]


def apply_crop_nonzero(g: Grid) -> Grid:
    h, w = grid_dims(g)
    if h == 0:
        return g
    min_r, max_r, min_c, max_c = h, 0, w, 0
    for r in range(h):
        for c in range(w):
            if g[r][c] != 0:
                min_r = min(min_r, r)
                max_r = max(max_r, r)
                min_c = min(min_c, c)
                max_c = max(max_c, c)
    if max_r < min_r:
        return [[0]]
    return [g[r][min_c : max_c + 1] for r in range(min_r, max_r + 1)]


def apply_tile(g: Grid, nh: int, nw: int) -> Grid:
    ih, iw = grid_dims(g)
    return [[g[y % ih][x % iw] for x in range(iw * nw)] for y in range(ih * nh)]


def apply_scale_pixel(g: Grid, k: int) -> Grid:
    ih, iw = grid_dims(g)
    return [[g[y // k][x // k] for x in range(iw * k)] for y in range(ih * k)]


def apply_op(g: Grid, op: Op) -> Grid:
    if isinstance(op, str):
        if op.startswith("geo:"):
            return apply_geo(op[4:], g)
        if op == "crop_nonzero":
            return apply_crop_nonzero(g)
        raise ValueError(op)
    kind, payload = op
    if kind == "color_map":
        return apply_color_map(g, dict(payload))
    if kind == "replace_all":
        a, b = payload
        return apply_replace_all(g, int(a), int(b))
    if kind == "tile":
        nh, nw = payload
        return apply_tile(g, nh, nw)
    if kind == "scale":
        return apply_scale_pixel(g, int(payload))
    raise ValueError(kind)


def apply_program(g: Grid, program: Sequence[Op]) -> Grid:
    out = grid_copy(g)
    for op in program:
        out = apply_op(out, op)
    return out


def hamming_loss(pred: Grid, gold: Grid) -> int:
    if grid_dims(pred) != grid_dims(gold):
        ha, wa = grid_dims(pred)
        hb, wb = grid_dims(gold)
        return 10_000 + abs(ha - hb) * 100 + abs(wa - wb) * 100
    loss = 0
    for ra, rb in zip(pred, gold):
        for a, b in zip(ra, rb):
            if a != b:
                loss += 1
    return loss


def total_score(program: Sequence[Op], examples: List[Tuple[Grid, Grid]]) -> int:
    s = 0
    for inp, out in examples:
        s += hamming_loss(apply_program(inp, program), out)
    return s


def program_receipt(program: Sequence[Op], examples: List[Tuple[Grid, Grid]], test_in: Grid) -> Dict[str, Any]:
    canon = json.dumps(
        {"ops": [op if isinstance(op, str) else [op[0], op[1]] for op in program]},
        sort_keys=True,
        separators=(",", ":"),
    )
    return {
        "program_canonical": canon,
        "program_hash": hashlib.blake2b(canon.encode(), digest_size=16).hexdigest(),
        "train_score": total_score(program, examples),
        "test_output": apply_program(test_in, program),
    }


def _primitive_catalog(examples: List[Tuple[Grid, Grid]]) -> List[Op]:
    ops: List[Op] = [f"geo:{n}" for n in _GEO]
    ops.append("crop_nonzero")
    cmap0 = _infer_color_map(examples[0][0], examples[0][1]) if examples else None
    if cmap0:
        t = tuple(sorted(cmap0.items()))
        ops.append(("color_map", t))
    rep = _infer_replace(examples[0][0], examples[0][1]) if examples else None
    if rep:
        ops.append(("replace_all", rep))
    for inp, out in examples[:1]:
        ih, iw = grid_dims(inp)
        oh, ow = grid_dims(out)
        if ih and iw and oh % ih == 0 and ow % iw == 0:
            ops.append(("tile", (oh // ih, ow // iw)))
        if ih and iw and oh % ih == 0 and ow % iw == 0 and oh // ih == ow // iw:
            ops.append(("scale", oh // ih))
    seen = set()
    uniq: List[Op] = []
    for o in ops:
        k = json.dumps(o if isinstance(o, str) else [o[0], o[1]], sort_keys=True)
        if k not in seen:
            seen.add(k)
            uniq.append(o)
    return uniq


def _infer_color_map(inp: Grid, out: Grid) -> Optional[Dict[int, int]]:
    if grid_dims(inp) != grid_dims(out):
        return None
    cmap: Dict[int, int] = {}
    for ri, ro in zip(inp, out):
        for ci, co in zip(ri, ro):
            if ci in cmap and cmap[ci] != co:
                return None
            cmap[ci] = co
    return cmap


def _infer_replace(inp: Grid, out: Grid) -> Optional[Tuple[int, int]]:
    if grid_dims(inp) != grid_dims(out):
        return None
    pairs = set()
    for ri, ro in zip(inp, out):
        for a, b in zip(ri, ro):
            if a != b:
                pairs.add((a, b))
    if len(pairs) == 1:
        return next(iter(pairs))
    return None


def beam_search_program(
    examples: List[Tuple[Grid, Grid]],
    max_depth: int = 4,
    beam_width: int = 24,
) -> List[Tuple[List[Op], int]]:
    """Palauttaa (ohjelma, score) parhausjärjestyksessä."""
    if not examples:
        return [([], 0)]
    catalog = _primitive_catalog(examples)
    beam: List[List[Op]] = [[]]
    scored: List[Tuple[List[Op], int]] = []

    for depth in range(max_depth + 1):
        candidates: List[Tuple[List[Op], int]] = []
        for prog in beam:
            sc = total_score(prog, examples)
            candidates.append((prog, sc))
            if sc == 0:
                scored.append((prog[:], sc))
        candidates.sort(key=lambda x: x[1])
        beam = [p for p, _ in candidates[:beam_width]]
        if depth == max_depth:
            break
        next_beam: List[List[Op]] = []
        for prog in beam:
            for op in catalog:
                np_ = prog + [op]
                next_beam.append(np_)
        if not next_beam:
            break
        next_beam.sort(key=lambda p: total_score(p, examples))
        beam = next_beam[: max(beam_width * 2, 32)]

    all_progs = scored + [(p, total_score(p, examples)) for p in beam]
    all_progs.sort(key=lambda x: x[1])
    best: Dict[str, List[Op]] = {}
    out: List[Tuple[List[Op], int]] = []
    for prog, sc in all_progs:
        key = json.dumps([str(x) for x in prog])
        if key in best:
            continue
        best[key] = prog
        out.append((prog, sc))
    return out


def counterexample_refine(
    program: List[Op],
    examples: List[Tuple[Grid, Grid]],
) -> Tuple[List[Op], Optional[int]]:
    """
    Jos ohjelma epäonnistuu jossain esimerkissä, yritä poistaa viimeisiä ops.
    Palauttaa (uusi_prog, index_of_first_fail tai None).
    """
    fail_idx: Optional[int] = None
    for i, (inp, gold) in enumerate(examples):
        if hamming_loss(apply_program(inp, program), gold) > 0:
            fail_idx = i
            break
    if fail_idx is None:
        return program, None
    for drop in range(1, min(len(program), 4) + 1):
        shorter = program[:-drop]
        if all(hamming_loss(apply_program(inp, shorter), gold) == 0 for inp, gold in examples):
            return shorter, fail_idx
    return program, fail_idx


def run_closed_loop(
    examples: List[Tuple[Grid, Grid]],
    test_input: Grid,
    *,
    max_depth: int = 4,
    beam_width: int = 24,
) -> Dict[str, Any]:
    t0 = time.perf_counter()
    ranked = beam_search_program(examples, max_depth=max_depth, beam_width=beam_width)
    best_prog, best_sc = ranked[0] if ranked else ([], 10**9)
    refined, fail_ex = counterexample_refine(best_prog, examples)
    if fail_ex is not None and refined != best_prog:
        best_prog, best_sc = refined, total_score(refined, examples)
    test_out = apply_program(test_input, best_prog)
    rec = program_receipt(best_prog, examples, test_input)
    wall = (time.perf_counter() - t0) * 1000
    return {
        "program": [str(x) for x in best_prog],
        "train_score": best_sc,
        "perfect": best_sc == 0,
        "counterexample_index": fail_ex,
        "refined": refined != ranked[0][0] if ranked else False,
        "test_output": test_out,
        "receipt": rec,
        "candidates_considered": len(ranked),
        "wall_ms": wall,
        "invariant": "1=1",
    }


def demo_tasks() -> List[Dict[str, Any]]:
    """Sisäiset ARC-tyyliset mikrotehtävät."""
    return [
        {
            "name": "rot90_chain",
            "examples": [
                ([[1, 2], [3, 4]], [[3, 1], [4, 2]]),
            ],
            "test": [[9, 8], [7, 6]],
            "expect_rule_contains": "rot90",
        },
        {
            "name": "color_map",
            "examples": [
                ([[1, 1], [2, 2]], [[5, 5], [2, 2]]),
            ],
            "test": [[1, 2]],
            "expect_score_zero": True,
        },
    ]


def run_demo_suite() -> Dict[str, Any]:
    results = []
    ok = 0
    for task in demo_tasks():
        ex = [(grid_copy(a), grid_copy(b)) for a, b in task["examples"]]
        ti = grid_copy(task["test"])
        r = run_closed_loop(ex, ti, max_depth=3, beam_width=16)
        good = r["perfect"]
        if task.get("expect_score_zero"):
            good = r["train_score"] == 0
        if task.get("expect_rule_contains"):
            good = any(task["expect_rule_contains"] in str(p) for p in r["program"])
        ok += int(good)
        results.append({"task": task["name"], "ok": good, **{k: r[k] for k in ("train_score", "program", "wall_ms")}})
    return {"tasks_ok": ok, "tasks_total": len(results), "results": results}


if __name__ == "__main__":
    import sys

    r = run_demo_suite()
    print(json.dumps(r, indent=2, ensure_ascii=False))
    sys.exit(0 if r["tasks_ok"] == r["tasks_total"] else 1)

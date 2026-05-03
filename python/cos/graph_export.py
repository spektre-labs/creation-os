# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Export :class:`~cos.graph.SigmaGraph` to JSON and Obsidian-flavoured Markdown."""
from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any, Dict, List, Optional, Set

from cos.graph import SigmaGraph, Triple

__all__ = [
    "GraphExport",
    "export_json",
    "export_obsidian",
    "load_graph_from_json",
    "triples_to_sigma_edges",
]


def _safe_name(s: str) -> str:
    return re.sub(r"[^\w\s-]", "", s, flags=re.UNICODE).strip().replace(" ", "_") or "entity"


def triples_to_sigma_edges(graph: SigmaGraph) -> List[Dict[str, Any]]:
    """σ per edge for viz (thickness ∝ reliability ≈ 1−σ in consumer)."""
    edges: List[Dict[str, Any]] = []
    for t in graph.triples.values():
        edges.append(
            {
                "source": t.subject,
                "target": t.object,
                "relation": t.relation,
                "sigma": round(float(t.sigma), 4),
                "reliability": round(max(0.0, min(1.0, 1.0 - float(t.sigma))), 4),
                "id": t.id,
            }
        )
    return edges


def export_json(graph: SigmaGraph, path: str | Path) -> Dict[str, Any]:
    """Write full graph triples + stats to JSON."""
    p = Path(path).expanduser().resolve()
    p.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "stats": graph.stats(),
        "sigma_edges": triples_to_sigma_edges(graph),
        "triples": [
            {
                "id": t.id,
                "subject": t.subject,
                "relation": t.relation,
                "object": t.object,
                "sigma": t.sigma,
                "source": t.source,
                "created": t.timestamp,
            }
            for t in graph.triples.values()
        ],
    }
    p.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"path": str(p), "triples": len(graph.triples)}


def load_graph_from_json(path: str | Path, graph: Optional[SigmaGraph] = None) -> SigmaGraph:
    """Hydrate a :class:`SigmaGraph` from :func:`export_json` output."""
    p = Path(path).expanduser().resolve()
    raw = json.loads(p.read_text(encoding="utf-8"))
    g = graph or SigmaGraph()
    for row in raw.get("triples", []):
        ts = row.get("created")
        tr = Triple(
            str(row["subject"]),
            str(row["relation"]),
            str(row["object"]),
            float(row.get("sigma", 0.2)),
            row.get("source"),
            timestamp=float(ts) if ts is not None else None,
        )
        g._insert_direct(tr)
    return g


def export_obsidian(graph: SigmaGraph, dir_path: str | Path) -> Dict[str, Any]:
    """One ``.md`` per entity; body lists incident edges with ``[[wiki-links]]``."""
    root = Path(dir_path).expanduser().resolve()
    root.mkdir(parents=True, exist_ok=True)
    entities: Dict[str, Set[str]] = {}
    for t in graph.triples.values():
        entities.setdefault(t.subject, set()).add(t.object)
        entities.setdefault(t.object, set()).add(t.subject)
    files = 0
    for ent, neighbours in entities.items():
        fname = _safe_name(ent) + ".md"
        lines = [f"# {ent}\n", "", "## links", ""]
        for n in sorted(neighbours):
            lines.append(f"- [[{_safe_name(n)}|{n}]]")
        lines.append("")
        (root / fname).write_text("\n".join(lines), encoding="utf-8")
        files += 1
    return {"dir": str(root), "files_written": files, "entities": len(entities)}


class GraphExport:
    """Obsidian / JSON export with σ-aware front matter and aggregate stats."""

    def __init__(self, graph: SigmaGraph) -> None:
        self.graph = graph

    def to_obsidian(self, output_dir: str | Path) -> None:
        out = Path(output_dir).expanduser().resolve()
        out.mkdir(parents=True, exist_ok=True)
        for entity in self.graph.entities():
            relations = self.graph.relations_of(entity)
            avg = self.graph.avg_sigma(entity)
            md = f"---\ntype: entity\nsigma_avg: {avg:.3f}\n---\n\n"
            md += f"# {entity}\n\n"
            if relations:
                md += "## Relations\n\n"
                for rel in relations:
                    target = rel["object"] if rel["subject"] == entity else rel["subject"]
                    md += f"- {rel['relation']}: [[{_safe_name(target)}|{target}]] (σ={rel['sigma']:.3f})\n"
            (out / f"{_safe_name(entity)}.md").write_text(md, encoding="utf-8")
        index = "# Knowledge Graph Index\n\n"
        for ent in sorted(self.graph.entities(), key=lambda x: x.lower()):
            index += f"- [[{_safe_name(ent)}|{ent}]]\n"
        (out / "INDEX.md").write_text(index, encoding="utf-8")

    def to_json(self, output_path: str | Path | None = None) -> Dict[str, Any]:
        data: Dict[str, Any] = {
            "entities": self.graph.entities(),
            "triples": self.graph.all_triples(),
            "stats": self.stats(),
        }
        if output_path is not None:
            outp = Path(output_path).expanduser().resolve()
            outp.parent.mkdir(parents=True, exist_ok=True)
            outp.write_text(json.dumps(data, indent=2, ensure_ascii=False, default=str), encoding="utf-8")
        return data

    def stats(self) -> Dict[str, Any]:
        triples = self.graph.all_triples()
        sigmas = [float(t["sigma"]) for t in triples if "sigma" in t]
        return {
            "entities": len(self.graph.entities()),
            "triples": len(triples),
            "avg_sigma": sum(sigmas) / max(len(sigmas), 1),
            "high_sigma_triples": sum(1 for s in sigmas if s > 0.5),
            "low_sigma_triples": sum(1 for s in sigmas if s < 0.2),
        }

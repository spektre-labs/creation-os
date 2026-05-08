# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-weighted graph visualisation (optional ``networkx`` + ``matplotlib``)."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from pathlib import Path
from typing import Any, Dict, Optional

from cos.graph import SigmaGraph

__all__ = ["visualize"]


def visualize(
    graph: SigmaGraph,
    path: Optional[str | Path] = None,
    *,
    figsize: tuple[float, float] = (10.0, 8.0),
) -> Dict[str, Any]:
    """
    Draw an undirected summary of the triple store; edge width scales with ``1 - σ``.
    Requires optional extras: ``pip install 'creation-os[viz]'``.
    """
    try:
        import matplotlib.pyplot as plt  # type: ignore[import-not-found]
        import networkx as nx  # type: ignore[import-not-found]
    except ImportError as exc:  # pragma: no cover
        raise ImportError(
            "graph visualisation needs optional deps: pip install 'creation-os[viz]' "
            f"({exc})"
        ) from exc

    g = nx.Graph()
    for t in graph.triples.values():
        g.add_edge(t.subject, t.object, sigma=float(t.sigma), rel=t.relation)
    pos = nx.spring_layout(g, seed=42) if len(g) else {}
    plt.figure(figsize=figsize)
    nx.draw_networkx_nodes(g, pos, node_size=400, node_color="#4a90d9", alpha=0.9)
    edges = list(g.edges(data=True))
    widths = []
    for _u, _v, data in edges:
        sig = float(data.get("sigma", 0.5))
        w = max(0.3, 4.0 * max(0.0, min(1.0, 1.0 - sig)))
        widths.append(w)
    nx.draw_networkx_edges(
        g, pos, width=widths, alpha=0.7, edge_color="#333333"
    )
    nx.draw_networkx_labels(g, pos, font_size=8)
    plt.axis("off")
    outp = Path(path).expanduser().resolve() if path else Path("sigma_graph.png")
    outp.parent.mkdir(parents=True, exist_ok=True)
    plt.savefig(outp, dpi=120, bbox_inches="tight")
    plt.close()
    return {"path": str(outp), "nodes": g.number_of_nodes(), "edges": g.number_of_edges()}

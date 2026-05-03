# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""σ-validated knowledge graph: triple store + multi-hop query."""

from __future__ import annotations

from cos.graph import SigmaGraph

kg = SigmaGraph()
kg.add("Marie Curie", "discovered", "radium", sigma=0.05)
kg.add("radium", "used_for", "cancer treatment", sigma=0.08)
path = kg.multi_hop("Marie Curie", "cancer treatment")
print(f"Path found: {path['found']}, hops: {path['hops']}")
if path.get("cumulative_sigma") is not None:
    print(f"Cumulative σ (path cost): {path['cumulative_sigma']}")

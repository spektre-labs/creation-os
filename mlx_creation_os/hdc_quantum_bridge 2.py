# -*- coding: utf-8 -*-
"""
HDC ↔ quantum-native vocabulary (conceptual bridge).

Classical GDA / BBHash / bundling in this repo are **digital** HDC instantiations.
When hardware supports native phase / superposition primitives, the **same** algebra
maps abstractly:

- **Bundling** (XOR-sum superposition metaphor) ↔ superposition of basis states
- **Binding** (role-filler) ↔ phase / controlled rotation patterns (platform-specific)
- **Permutation** ↔ qFT-style mixing in some encodings

This module holds **names and documentation only** — no quantum runtime dependency.
Substrate may change; σ-kernel semantics remain the contract boundary.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List

HDC_OPERATION_MAP: List[Dict[str, str]] = [
    {
        "hdc_op": "bundle_xor_superposition",
        "quantum_metaphor": "superposition_of_holographic_symbols",
        "repo_anchor": "GDA / BBHash / hdc bridge modules",
    },
    {
        "hdc_op": "bind_role_filler",
        "quantum_metaphor": "phase_oracle_on_subspace",
        "repo_anchor": "hyperdimensional_copu / quantum_hdc_ops (classical sim)",
    },
    {
        "hdc_op": "permute_sequence",
        "quantum_metaphor": "unitary_mixing_resembling_QFT_structure",
        "repo_anchor": "permutation in HDC pipelines",
    },
]


def quantum_readiness_receipt() -> Dict[str, Any]:
    return {
        "status": "classical_hdc_shipped_quantum_substrate_optional",
        "operations": HDC_OPERATION_MAP,
        "invariant": "sigma_kernel_semantics_stable_across_substrate",
    }

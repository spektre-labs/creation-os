# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Creation OS — σ-aware cognitive architecture (`cos` package; local AI runtime).

Heavy subsystems load on first attribute access (PEP 562 / PEP 810-style lazy exports).
:class:`~cos.sigma_gate.SigmaGate` stays a direct import so ``cos score`` can stay light.
"""

from __future__ import annotations

import importlib
from typing import Any

from cos.sigma_gate import SigmaGate

__version__ = "1.0.0"

_LAZY_IMPORT_MAP: dict[str, str] = {
    "DEFAULT_CONFIG": "cos.config",
    "SigmaConfig": "cos.config",
    "EmbodiedController": "cos.embodied",
    "SensorFusion": "cos.embodied",
    "EpistemicAgent": "cos.epistemic",
    "EnergyAware": "cos.energy",
    "Fabric": "cos.fabric",
    "FederatedNode": "cos.federated",
    "FederatedSigma": "cos.federated",
    "SigmaFabric": "cos.fabric",
    "Pipeline": "cos.pipeline",
    "PipelineResult": "cos.pipeline",
    "Mega": "cos.mega",
    "ModuleSpec": "cos.architect",
    "SigmaChat": "cos.chat",
    "SigmaCascade": "cos.cascade_router",
    "SigmaArchitect": "cos.architect",
    "SigmaCodeGen": "cos.codegen",
    "SigmaVoice": "cos.voice",
    "SigmaWorld": "cos.world",
    "SigmaConscious": "cos.conscious",
    "SigmaDrive": "cos.drive",
    "SigmaSocial": "cos.social",
    "PearlLadder": "cos.causal",
    "SigmaTTT": "cos.ttt",
    "SigmaEvolve": "cos.evolve",
    "ActiveInference": "cos.active_inference",
    "PredictiveCoding": "cos.predictive",
    "SigmaFactorGraph": "cos.factor_graph",
    "MarkovBlanket": "cos.blanket",
    "Autopoietic": "cos.autopoiesis",
    "SigmaGrounding": "cos.grounding",
    "QuantumCognition": "cos.quantum_cognition",
    "StrangeLoop": "cos.strange_loop",
    "AutonomousAgent": "cos.autonomous",
    "ThinkBudget": "cos.think_budget",
    "SigmaMoE": "cos.sigma_moe",
    "CostManager": "cos.cost",
    "SigmaSpeculative": "cos.speculative",
    "SigmaKVCache": "cos.kv_cache",
    "LivingWeights": "cos.living_weights",
    "SigmaMoral": "cos.moral",
}

__all__ = [
    "ActiveInference",
    "AutonomousAgent",
    "Autopoietic",
    "CostManager",
    "DEFAULT_CONFIG",
    "EmbodiedController",
    "EpistemicAgent",
    "EnergyAware",
    "Fabric",
    "FederatedNode",
    "FederatedSigma",
    "LivingWeights",
    "MarkovBlanket",
    "Mega",
    "ModuleSpec",
    "PearlLadder",
    "Pipeline",
    "PipelineResult",
    "PredictiveCoding",
    "QuantumCognition",
    "SensorFusion",
    "SigmaCascade",
    "SigmaArchitect",
    "SigmaChat",
    "SigmaCodeGen",
    "SigmaConscious",
    "SigmaConfig",
    "SigmaDrive",
    "SigmaEvolve",
    "SigmaFabric",
    "SigmaFactorGraph",
    "SigmaGate",
    "SigmaGrounding",
    "SigmaKVCache",
    "SigmaMoE",
    "SigmaMoral",
    "SigmaSocial",
    "SigmaSpeculative",
    "SigmaTTT",
    "SigmaVoice",
    "SigmaWorld",
    "StrangeLoop",
    "ThinkBudget",
    "__version__",
]


def __getattr__(name: str) -> Any:
    modname = _LAZY_IMPORT_MAP.get(name)
    if modname is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    mod = importlib.import_module(modname)
    value = getattr(mod, name)
    globals()[name] = value
    return value


def __dir__() -> list[str]:
    return sorted(__all__)

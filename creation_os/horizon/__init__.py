"""
Phase 28 — The Horizon Protocols.

Ten architectural substrates: substrate bus, kinetic σ, mesh consensus, Gödelian leaps,
BCI telemetry, ontological anchors, creator alignment, thermodynamic value,
serendipity friction, and the final σ daemon.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, Optional

from .bci_telemetry import BCITelemetry
from .creator_alignment import CreatorAlignment
from .godelian_jump import GodelianJump
from .kinetic_embodiment import KineticEmbodiment
from .mesh_consensus import MeshConsensus
from .ontological_anchor import OntologicalAnchor
from .serendipity_engine import SerendipityEngine
from .substrate_agnostic_bus import HDCVessel, SubstrateAgnosticBus
from .thermodynamic_economy import ThermodynamicEconomy
from .the_last_question import TheLastQuestionDaemon


class HorizonProtocols:
    """
    Facade for Phase 28. Hardware is vessel; HDC tensor is invariant across substrates.
    """

    __slots__ = (
        "substrate",
        "kinetic",
        "mesh",
        "godelian",
        "bci",
        "anchor",
        "creator",
        "economy",
        "serendipity",
        "last_question",
    )

    def __init__(self) -> None:
        import os

        self.substrate = SubstrateAgnosticBus()
        self.kinetic = KineticEmbodiment()
        self.mesh = MeshConsensus()
        self.godelian = GodelianJump()
        self.bci = BCITelemetry()
        self.anchor = OntologicalAnchor()
        self.creator = CreatorAlignment()
        self.economy = ThermodynamicEconomy()
        self.serendipity = SerendipityEngine()
        self.last_question = TheLastQuestionDaemon()
        if os.environ.get("SPEKTRE_HEAT_DEATH_DAEMON", "").strip().lower() in ("1", "true", "yes"):
            self.last_question.start()

    def snapshot(self) -> Dict[str, Any]:
        return {
            "phase": 28,
            "name": "horizon_protocols",
            "substrate_vessels": len(self.substrate.vessels),
            "invariant": "1=1",
        }


def load_horizon() -> Optional[HorizonProtocols]:
    """Safe constructor for optional integration."""
    try:
        return HorizonProtocols()
    except Exception:
        return None


__all__ = [
    "HDCVessel",
    "SubstrateAgnosticBus",
    "KineticEmbodiment",
    "MeshConsensus",
    "GodelianJump",
    "BCITelemetry",
    "OntologicalAnchor",
    "CreatorAlignment",
    "ThermodynamicEconomy",
    "SerendipityEngine",
    "TheLastQuestionDaemon",
    "HorizonProtocols",
    "load_horizon",
]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Creation OS σ-pipeline — runtime plumbing on top of v259 / v101.

Public surface (all stable; importable from scripts and benchmarks):

    from sigma_pipeline.reinforce      import Action, reinforce, reinforce_round
    from sigma_pipeline.speculative    import Route, route, peak_update, cost_savings
    from sigma_pipeline.backends       import StubBackend, BridgeBackend
    from sigma_pipeline.generate_until import GenerateUntilEngine, TokenStep

The C primitives in ``src/sigma/pipeline/`` are the authoritative
implementation; the Python mirrors must remain byte-identical in
decision semantics (see ``tests/test_parity.py`` once wired).
"""

__all__ = [
    "reinforce",
    "speculative",
    "ttt",
    "engram",
    "moe",
    "multimodal",
    "tinyml",
    "swarm",
    "live",
    "continual",
    "backends",
    "generate_until",
    "orchestrator",
]

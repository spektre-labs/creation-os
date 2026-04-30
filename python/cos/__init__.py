"""Creation OS `cos` Python package (σ-gate kernels and harness helpers).

This ``__init__`` is intentionally **lightweight**: it does not eagerly import the full
stack (that pattern broke when optional submodules were empty / out of sync).

Import what you need from submodules, e.g.::

    from cos.sigma_gate import SigmaGate
    from cos.sigma_hide import SigmaHIDE
"""

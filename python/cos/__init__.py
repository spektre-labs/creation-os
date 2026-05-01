# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Creation OS `cos` Python package (σ-gate kernels and harness helpers).

This ``__init__`` is intentionally **lightweight**: it does not eagerly import the full
stack (that pattern broke when optional submodules were empty / out of sync).

Import what you need from submodules, e.g.::

    from cos.sigma_gate import SigmaGate
    from cos.sigma_hide import SigmaHIDE
"""

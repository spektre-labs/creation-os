# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""Optional extensions via **PEP 621** / ``importlib.metadata`` **entry points**.

Third-party wheels can register probes, lab modules, or CLI hooks without forking this
repo. After ``pip install <your-add-on>``, entry points in groups below are visible to
:class:`PluginRegistry` (**no extra config** in Creation OS).

**Groups**

- ``cos.probes`` — custom σ-surface probes (callables or probe-like objects).
- ``cos.modules`` — optional cognitive / lab modules.
- ``cos.cli`` — optional CLI entry callables (advanced; integrate via your own dispatch).

Example (add-on ``pyproject.toml``)::

    [project.entry-points."cos.probes"]
    medical = "cos_medical.probe:MedicalProbe"

This is **packaging plumbing** only: it does not change ``sigma_gate.h`` or core gate math.
"""
from __future__ import annotations

import importlib
import importlib.metadata
from typing import Any, Dict, List, Mapping, cast

__all__ = ["PluginRegistry"]


def _entry_points_in_group(group: str) -> tuple[importlib.metadata.EntryPoint, ...]:
    """Return installed entry points for ``group`` (empty if metadata is unavailable)."""
    try:
        eps = importlib.metadata.entry_points()
        selected = eps.select(group=group)
        return tuple(selected)
    except Exception:
        return ()


class PluginRegistry:
    """Discover and import Creation OS extensions advertised via entry points."""

    GROUPS: Mapping[str, str] = {
        "cos.probes": "Custom σ-gate probes",
        "cos.modules": "Custom cognitive modules",
        "cos.cli": "Custom CLI commands",
    }

    def __init__(self) -> None:
        # Flat map ``f\"{group}:{name}\"`` so different groups never collide.
        self.loaded: Dict[str, Any] = {}

    def discover(self, group: str = "cos.probes") -> List[Dict[str, str]]:
        """List installed entry points for ``group`` (name + import target)."""
        plugins: List[Dict[str, str]] = []
        for ep in _entry_points_in_group(group):
            plugins.append(
                {
                    "name": ep.name,
                    "module": ep.value,
                    "group": group,
                }
            )
        return plugins

    def load(self, group: str = "cos.probes") -> Dict[str, Any]:
        """Import every entry in ``group``; store under keys ``group:name``."""
        bucket: Dict[str, Any] = {}
        for p in self.discover(group):
            ep_name = p["name"]
            key = f"{group}:{ep_name}"
            module_str, _, attr = p["module"].partition(":")
            module_str = module_str.strip()
            attr = attr.strip()
            try:
                mod = importlib.import_module(module_str)
                obj: Any = getattr(mod, attr) if attr else mod
                bucket[ep_name] = obj
                self.loaded[key] = obj
            except Exception as exc:  # noqa: BLE001 — plugin load is best-effort
                err = {"error": str(exc)}
                bucket[ep_name] = err
                self.loaded[key] = err
        return bucket

    def get_probe(self, name: str) -> Any:
        """Return a loaded probe object for ``name`` in group ``cos.probes``."""
        return self.loaded.get(f"cos.probes:{name}")

    def list_all(self) -> Dict[str, Any]:
        """Summarize metadata for every known group (discovery only; no imports)."""
        result: Dict[str, Any] = {}
        for group, desc in self.GROUPS.items():
            plugins = self.discover(group)
            result[group] = {
                "description": desc,
                "plugins": plugins,
                "count": len(plugins),
            }
        return cast(Dict[str, Any], result)

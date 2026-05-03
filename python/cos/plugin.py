# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-plugin — third-party hook registry (lab).

Hooks are **in-process** Python callables. ``sandbox_execute`` uses :class:`cos.sandbox.SigmaSandbox`
(single-expression eval). ``marketplace_install`` ties to :class:`cos.marketplace.SigmaMarketplace`
probe ids. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import importlib.util
from typing import Any, Callable, Dict, List, Mapping, Optional

__all__ = ["SigmaPlugin"]

HookFn = Callable[..., Any]

HOOK_POINTS = (
    "pre_score",
    "post_score",
    "pre_cascade",
    "post_cascade",
    "on_accept",
    "on_rethink",
    "on_abstain",
)


class SigmaPlugin:
    """Register callbacks per hook point; optional marketplace-backed install."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._registry: Dict[str, Dict[str, Any]] = {}

    def register(self, name: str, hook_point: str, callback: HookFn) -> Dict[str, Any]:
        hp = str(hook_point).strip()
        if hp not in HOOK_POINTS:
            raise ValueError(f"hook_point must be one of {HOOK_POINTS}")
        prof = self.plugin_sigma({"name": name, "hook_point": hp}, self.gate)
        rec = {
            "name": str(name),
            "hook_point": hp,
            "callback": callback,
            "disabled": False,
            "sigma_profile": prof,
        }
        self._registry[str(name)] = rec
        return {"registered": True, "name": str(name), "sigma_profile": prof}

    def load_from_module(self, module_path: str) -> Dict[str, Any]:
        """Execute ``module_path`` and call ``register_sigma_plugin(plug)`` if present."""
        path = str(module_path)
        spec = importlib.util.spec_from_file_location("_cos_sigma_plugin", path)
        if spec is None or spec.loader is None:
            return {"ok": False, "error": "invalid_module_spec"}
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        fn = getattr(mod, "register_sigma_plugin", None)
        if not callable(fn):
            return {"ok": False, "error": "missing register_sigma_plugin"}
        fn(self)
        return {"ok": True, "path": path}

    @staticmethod
    def plugin_sigma(plugin: Mapping[str, Any], gate: Any) -> Dict[str, Any]:
        """σ metadata for the plugin record (name + hook)."""
        blob = f"{plugin.get('name')}|{plugin.get('hook_point')}"
        sigma, verdict = gate.score("plugin_registry", blob)
        return {"sigma": round(float(sigma), 6), "verdict": str(verdict)}

    def sandbox_execute(self, plugin: Mapping[str, Any], input_data: str) -> Dict[str, Any]:
        from cos.sandbox import SigmaSandbox

        cb = plugin.get("callback")
        if not callable(cb):
            return {"ok": False, "error": "plugin_missing_callback"}
        sandbox = SigmaSandbox(gate=self.gate)
        try:
            expr = str(cb(str(input_data)))
        except Exception as e:
            return {"ok": False, "error": str(e)}
        return sandbox.execute_safe(expr)

    def marketplace_install(
        self,
        plugin_id: str,
        marketplace: Optional[Any] = None,
        *,
        hook_point: str = "post_score",
    ) -> Dict[str, Any]:
        from cos.marketplace import SigmaMarketplace

        m = marketplace or SigmaMarketplace()
        d = m.download_probe(str(plugin_id))
        if not d.get("ok"):
            return {"ok": False, "error": d.get("error", "download_failed")}
        pid = str(plugin_id)

        def _probe_cb(ctx: str) -> str:
            return repr({"probe_id": pid, "ctx_head": str(ctx)[:48]})

        name = f"marketplace_{pid}"
        self.register(name, hook_point, _probe_cb)
        return {"ok": True, "name": name, "sha256": d.get("sha256")}

    def list_plugins(self) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        for rec in self._registry.values():
            out.append(
                {
                    "name": rec["name"],
                    "hook_point": rec["hook_point"],
                    "disabled": bool(rec["disabled"]),
                    "sigma_profile": rec["sigma_profile"],
                }
            )
        return sorted(out, key=lambda x: x["name"])

    def disable(self, plugin_name: str) -> Dict[str, Any]:
        rec = self._registry.get(str(plugin_name))
        if not rec:
            return {"ok": False, "error": "not_found"}
        rec["disabled"] = True
        return {"ok": True, "name": str(plugin_name)}

    def dispatch(self, hook_point: str, *args: Any, **kwargs: Any) -> List[Any]:
        """Run all non-disabled callbacks for a hook (utility for integrations)."""
        hp = str(hook_point)
        results: List[Any] = []
        for rec in self._registry.values():
            if rec["disabled"] or rec["hook_point"] != hp:
                continue
            results.append(rec["callback"](*args, **kwargs))
        return results

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-RBAC — role templates with σ-budget (queries / verdicts per role).

Lab governance helper for :mod:`cos.sigma_team`. Not a substitute for enterprise IdP.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional


class SigmaRBAC:
    """Default roles: permissions, optional σ-budget (-1 = unlimited), cascade depth cap."""

    ROLES: Dict[str, Dict[str, Any]] = {
        "admin": {
            "permissions": ["read", "write", "query", "admin", "audit", "configure"],
            "sigma_budget": -1,
            "max_cascade_level": 5,
        },
        "developer": {
            "permissions": ["read", "write", "query"],
            "sigma_budget": 5000,
            "max_cascade_level": 5,
        },
        "analyst": {
            "permissions": ["read", "query"],
            "sigma_budget": 2000,
            "max_cascade_level": 3,
        },
        "viewer": {
            "permissions": ["read"],
            "sigma_budget": 100,
            "max_cascade_level": 1,
        },
        "member": {
            "permissions": ["read", "query"],
            "sigma_budget": 1000,
            "max_cascade_level": 3,
        },
    }

    def role_defaults(self, role: str) -> Dict[str, Any]:
        return self.ROLES.get(role, dict(self.ROLES["viewer"]))

    def member_record(
        self,
        role: str,
        *,
        permissions: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        base = self.role_defaults(role)
        perms = list(permissions) if permissions is not None else list(base["permissions"])
        budget = int(base["sigma_budget"])
        return {
            "role": role,
            "permissions": perms,
            "sigma_budget": budget,
            "max_cascade_level": int(base.get("max_cascade_level", 1)),
            "used": 0,
        }

    def check_permission(self, user: Dict[str, Any], action: str) -> bool:
        role = self.role_defaults(str(user.get("role", "viewer")))
        allowed = set(str(x) for x in user.get("permissions") or role["permissions"])
        return str(action) in allowed

    def check_budget(self, user: Dict[str, Any]) -> bool:
        role = self.role_defaults(str(user.get("role", "viewer")))
        limit = int(user.get("sigma_budget", role["sigma_budget"]))
        if limit < 0:
            return True
        return int(user.get("used", 0)) < limit


__all__ = ["SigmaRBAC"]

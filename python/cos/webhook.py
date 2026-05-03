# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-webhook — subscription registry + HMAC signing + batching (no outbound HTTP in core).

Wire ``deliver_*`` in your worker to POST signed payloads. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import hashlib
import hmac
import json
import time
import uuid
from typing import Any, Dict, List, Mapping, Optional, Sequence, Union

__all__ = ["SigmaWebhook"]

Payload = Union[str, bytes, Mapping[str, Any]]


class SigmaWebhook:
    """Register URLs per event; sign payloads; coalesce events for rate limits."""

    def __init__(self) -> None:
        self._subs: List[Dict[str, Any]] = []
        self._outbox: List[Dict[str, Any]] = []
        self._fail_counts: Dict[str, int] = {}

    def _add(self, event: str, url: str, **extra: Any) -> str:
        sid = str(uuid.uuid4())[:18]
        self._subs.append({"id": sid, "event": event, "url": str(url), **extra})
        return sid

    def on_abstain(self, url: str, payload_template: str) -> str:
        return self._add("abstain", url, payload_template=str(payload_template))

    def on_drift(self, url: str, threshold: float) -> str:
        return self._add("drift", url, threshold=float(threshold))

    def on_incident(self, url: str) -> str:
        return self._add("incident", url)

    def on_version_change(self, url: str) -> str:
        return self._add("version_change", url)

    @staticmethod
    def retry_policy(max_retries: int, backoff_seconds: float) -> Dict[str, Any]:
        return {"max_retries": int(max_retries), "backoff_seconds": float(backoff_seconds)}

    @staticmethod
    def signature(payload: Payload, secret: str) -> str:
        if isinstance(payload, Mapping):
            body = json.dumps(dict(payload), sort_keys=True, default=str).encode("utf-8")
        elif isinstance(payload, str):
            body = payload.encode("utf-8")
        else:
            body = bytes(payload)
        return hmac.new(secret.encode("utf-8"), body, hashlib.sha256).hexdigest()

    def batch_events(self, events: Sequence[Mapping[str, Any]], interval_seconds: float) -> List[Dict[str, Any]]:
        """Group consecutive events within ``interval_seconds`` (monotonic clock lab)."""
        if not events:
            return []
        interval = float(interval_seconds)
        batches: List[Dict[str, Any]] = []
        current: List[Mapping[str, Any]] = []
        last_t: Optional[float] = None
        for ev in events:
            t = float(ev.get("t", time.monotonic()))
            if last_t is None or (t - last_t) <= interval:
                current.append(ev)
            else:
                batches.append({"events": list(current), "count": len(current)})
                current = [ev]
            last_t = t
        if current:
            batches.append({"events": current, "count": len(current)})
        return batches

    def record_delivery(self, sub_id: str, payload: Mapping[str, Any], *, success: bool) -> None:
        self._outbox.append({"sub_id": sub_id, "payload": dict(payload), "success": bool(success), "t": time.monotonic()})
        if not success:
            self._fail_counts[sub_id] = self._fail_counts.get(sub_id, 0) + 1
        else:
            self._fail_counts[sub_id] = 0

    def subscriptions(self) -> List[Dict[str, Any]]:
        return list(self._subs)

    def delete_subscription(self, sub_id: str) -> bool:
        before = len(self._subs)
        self._subs = [s for s in self._subs if s.get("id") != sub_id]
        return len(self._subs) < before

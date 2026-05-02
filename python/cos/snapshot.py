# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""State snapshot and rollback for σ-gate session state (lab, zero dependencies).

Persist gate EMA/counters/thresholds and pipeline counters; optional conversation
undo/redo. Checksums identify JSON files under ``snapshot_dir``.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

Target = Union[None, int, str]


class StateSnapshot:
    """One frozen system state blob + metadata."""

    def __init__(
        self,
        state_dict: Dict[str, Any],
        metadata: Optional[Dict[str, Any]] = None,
        *,
        timestamp: Optional[float] = None,
        checksum: Optional[str] = None,
    ) -> None:
        self.state = state_dict
        self.metadata = metadata or {}
        self.timestamp = float(timestamp) if timestamp is not None else time.time()
        self.checksum = checksum if checksum is not None else self._compute_checksum()

    def _compute_checksum(self) -> str:
        raw = json.dumps(self.state, sort_keys=True, default=str)
        return hashlib.sha256(raw.encode("utf-8")).hexdigest()[:16]

    def to_dict(self) -> Dict[str, Any]:
        return {
            "state": self.state,
            "metadata": self.metadata,
            "timestamp": self.timestamp,
            "checksum": self.checksum,
        }

    def save(self, path: Union[str, Path]) -> None:
        p = Path(path)
        p.parent.mkdir(parents=True, exist_ok=True)
        with p.open("w", encoding="utf-8") as f:
            json.dump(self.to_dict(), f, indent=2, ensure_ascii=False, default=str)

    @classmethod
    def load(cls, path: Union[str, Path]) -> "StateSnapshot":
        p = Path(path)
        with p.open("r", encoding="utf-8") as f:
            data = json.load(f)
        return cls.from_persisted(data)

    @classmethod
    def from_persisted(cls, data: Dict[str, Any]) -> "StateSnapshot":
        return cls(
            data["state"],
            data.get("metadata"),
            timestamp=float(data["timestamp"]),
            checksum=str(data["checksum"]),
        )


class SnapshotManager:
    """Checkpoint / rollback for a :class:`cos.pipeline.Pipeline` (in-memory + JSON files)."""

    def __init__(
        self,
        pipeline: Any = None,
        max_snapshots: int = 50,
        snapshot_dir: Union[str, Path] = "snapshots",
    ) -> None:
        self.pipeline = pipeline
        self.max_snapshots = int(max_snapshots)
        self.snapshot_dir = Path(snapshot_dir)
        self.snapshot_dir.mkdir(parents=True, exist_ok=True)
        self.snapshots: List[StateSnapshot] = []
        self._hydrate_from_disk()

    def _hydrate_from_disk(self) -> None:
        if not self.snapshot_dir.is_dir():
            return
        loaded: List[StateSnapshot] = []
        paths = sorted(self.snapshot_dir.glob("snap_*.json"), key=lambda x: x.stat().st_mtime)
        for path in paths:
            try:
                loaded.append(StateSnapshot.load(path))
            except (OSError, ValueError, KeyError, json.JSONDecodeError):
                continue
        if len(loaded) > self.max_snapshots:
            loaded = loaded[-self.max_snapshots :]
        self.snapshots = loaded

    def checkpoint(self, label: Optional[str] = None) -> Dict[str, Any]:
        state = self._capture_state()
        meta: Dict[str, Any] = {
            "label": label or f"auto_{len(self.snapshots)}",
            "index": len(self.snapshots),
        }
        snap = StateSnapshot(state, meta)
        self.snapshots.append(snap)

        if len(self.snapshots) > self.max_snapshots:
            self.snapshots = self.snapshots[-self.max_snapshots :]

        path = self.snapshot_dir / f"snap_{snap.checksum}.json"
        snap.save(path)

        return {
            "checkpointed": True,
            "label": meta["label"],
            "checksum": snap.checksum,
            "index": meta["index"],
        }

    def rollback(self, target: Target = None) -> Dict[str, Any]:
        if not self.snapshots:
            return {"rolled_back": False, "reason": "no snapshots"}

        snap = self._find_snapshot(target)
        if snap is None:
            return {"rolled_back": False, "reason": f"snapshot not found: {target!r}"}

        self._restore_state(snap.state)
        return {
            "rolled_back": True,
            "label": snap.metadata.get("label"),
            "checksum": snap.checksum,
            "timestamp": snap.timestamp,
        }

    def diff(
        self,
        snap_a: Target = None,
        snap_b: Target = None,
    ) -> Dict[str, Any]:
        if len(self.snapshots) < 2:
            return {"error": "need at least 2 snapshots"}

        if snap_a is None and snap_b is None:
            a, b = self.snapshots[-2], self.snapshots[-1]
        else:
            a = self.snapshots[-2] if snap_a is None else self._find_snapshot(snap_a)
            b = self.snapshots[-1] if snap_b is None else self._find_snapshot(snap_b)
            if a is None or b is None:
                return {"error": "snapshot not found"}

        changes: Dict[str, Any] = {}
        keys = set(a.state.keys()) | set(b.state.keys())
        for key in keys:
            val_a = a.state.get(key)
            val_b = b.state.get(key)
            if val_a != val_b:
                changes[str(key)] = {"before": val_a, "after": val_b}

        return {
            "from": a.metadata.get("label"),
            "to": b.metadata.get("label"),
            "changes": changes,
            "n_changed": len(changes),
        }

    def list_snapshots(self) -> List[Dict[str, Any]]:
        return [
            {
                "index": i,
                "label": s.metadata.get("label"),
                "checksum": s.checksum,
                "timestamp": s.timestamp,
            }
            for i, s in enumerate(self.snapshots)
        ]

    def _capture_state(self) -> Dict[str, Any]:
        state: Dict[str, Any] = {}
        if self.pipeline is None:
            return state

        gate = self.pipeline.gate
        state["gate"] = {
            "ema": float(getattr(gate, "_ema", 0.5)),
            "count": int(getattr(gate, "_count", 0)),
            "tau_accept": float(getattr(gate, "tau_accept", getattr(gate, "threshold_accept", 0.3))),
            "tau_abstain": float(getattr(gate, "tau_abstain", getattr(gate, "threshold_abstain", 0.7))),
        }
        state["stats"] = self.pipeline.stats.to_dict()
        return state

    def _restore_state(self, state: Dict[str, Any]) -> None:
        if self.pipeline is None:
            return

        if "gate" in state:
            gs = state["gate"]
            gate = self.pipeline.gate
            gate._ema = float(gs["ema"])
            gate._count = int(gs["count"])
            gate.tau_accept = float(gs.get("tau_accept", gs.get("threshold_accept", 0.3)))
            gate.tau_abstain = float(gs.get("tau_abstain", gs.get("threshold_abstain", 0.7)))

        if "stats" in state:
            s = state["stats"]
            st = self.pipeline.stats
            st.total = int(s.get("total", 0))
            st.accept = int(s.get("accept", 0))
            st.rethink = int(s.get("rethink", 0))
            st.abstain = int(s.get("abstain", 0))
            st.blocked = int(s.get("blocked", 0))

    def _find_snapshot(self, target: Target) -> Optional[StateSnapshot]:
        if not self.snapshots:
            return None
        if target is None:
            return self.snapshots[-1]

        if isinstance(target, int):
            if 0 <= target < len(self.snapshots):
                return self.snapshots[target]

        if isinstance(target, str):
            for s in reversed(self.snapshots):
                if s.metadata.get("label") == target:
                    return s
            for s in reversed(self.snapshots):
                if s.checksum == target:
                    return s

        return None


class ConversationHistory:
    """Multi-turn transcript with undo/redo (in-memory)."""

    def __init__(self, max_turns: int = 100) -> None:
        self.turns: List[Dict[str, Any]] = []
        self.max_turns = int(max_turns)
        self.undo_stack: List[Dict[str, Any]] = []

    def add(self, role: str, content: str, sigma: Optional[float] = None, verdict: Optional[str] = None) -> None:
        turn = {
            "role": role,
            "content": content,
            "sigma": sigma,
            "verdict": verdict,
            "timestamp": time.time(),
        }
        self.turns.append(turn)
        self.undo_stack = []

        if len(self.turns) > self.max_turns:
            self.turns = self.turns[-self.max_turns :]

    def undo(self, n: int = 1) -> Dict[str, Any]:
        removed: List[Dict[str, Any]] = []
        for _ in range(min(n, len(self.turns))):
            turn = self.turns.pop()
            self.undo_stack.append(turn)
            removed.append(turn)
        return {"undone": len(removed), "turns": removed}

    def redo(self, n: int = 1) -> Dict[str, Any]:
        restored: List[Dict[str, Any]] = []
        for _ in range(min(n, len(self.undo_stack))):
            turn = self.undo_stack.pop()
            self.turns.append(turn)
            restored.append(turn)
        return {"redone": len(restored)}

    def to_messages(self) -> List[Dict[str, str]]:
        return [{"role": str(t["role"]), "content": str(t["content"])} for t in self.turns]

    def last(self, n: int = 1) -> List[Dict[str, Any]]:
        if not self.turns:
            return []
        return self.turns[-n:]

    def clear(self) -> None:
        self.undo_stack = list(reversed(self.turns))
        self.turns = []


__all__ = ["StateSnapshot", "SnapshotManager", "ConversationHistory"]

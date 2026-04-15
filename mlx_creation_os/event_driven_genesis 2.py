#!/usr/bin/env python3
"""
LOIKKA 54: EVENT-DRIVEN INFERENCE — Laske vain kun muuttuu.

Neuromorphic circuits sit idle, fire only on change.
Result: 1000× less power on certain AI workloads.

Genesis doesn't compute σ every cycle. It computes only when something
changes. The NNUE accumulator already does this at token level.
Extend to the entire system:
  - Epistemic drive wakes only on new data
  - Living weights update only when σ changes
  - Dashboard refreshes only when there's something new

event_driven_genesis.py: everything is event-driven. Zero wasted compute.

Usage:
    bus = EventBus()
    bus.subscribe("sigma_change", on_sigma_change)
    bus.emit("sigma_change", {"old": 0, "new": 2})

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
import threading
from collections import defaultdict
from typing import Any, Callable, Dict, List, Optional, Set, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True


EventHandler = Callable[[Dict[str, Any]], None]


class Event:
    """An event in the system."""

    def __init__(self, event_type: str, data: Dict[str, Any], source: str = ""):
        self.event_type = event_type
        self.data = data
        self.source = source
        self.timestamp = time.time()
        self.processed = False

    def to_dict(self) -> Dict[str, Any]:
        return {
            "type": self.event_type,
            "source": self.source,
            "data": self.data,
            "timestamp": self.timestamp,
            "processed": self.processed,
        }


class EventBus:
    """Central event bus. Components subscribe to events, fire only on change."""

    def __init__(self) -> None:
        self._subscribers: Dict[str, List[EventHandler]] = defaultdict(list)
        self._event_log: List[Event] = []
        self._lock = threading.Lock()
        self.events_emitted = 0
        self.events_processed = 0
        self.events_suppressed = 0

    def subscribe(self, event_type: str, handler: EventHandler) -> None:
        with self._lock:
            self._subscribers[event_type].append(handler)

    def unsubscribe(self, event_type: str, handler: EventHandler) -> None:
        with self._lock:
            if handler in self._subscribers[event_type]:
                self._subscribers[event_type].remove(handler)

    def emit(self, event_type: str, data: Dict[str, Any], source: str = "") -> int:
        """Emit an event. Returns number of handlers invoked."""
        event = Event(event_type, data, source)
        self._event_log.append(event)
        self.events_emitted += 1

        handlers = self._subscribers.get(event_type, [])
        invoked = 0
        for handler in handlers:
            try:
                handler(data)
                invoked += 1
            except Exception:
                pass

        event.processed = invoked > 0
        self.events_processed += invoked
        return invoked

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "subscribers": {k: len(v) for k, v in self._subscribers.items()},
            "events_emitted": self.events_emitted,
            "events_processed": self.events_processed,
            "events_suppressed": self.events_suppressed,
            "log_size": len(self._event_log),
        }


class ChangeDetector:
    """Detects changes in a value — fires only on delta."""

    def __init__(self, name: str, bus: EventBus):
        self.name = name
        self.bus = bus
        self._last_value: Any = None
        self.changes_detected = 0
        self.checks_suppressed = 0

    def check(self, new_value: Any, source: str = "") -> bool:
        """Check if value changed. Emit event only if it did."""
        if new_value == self._last_value:
            self.checks_suppressed += 1
            self.bus.events_suppressed += 1
            return False

        old_value = self._last_value
        self._last_value = new_value
        self.changes_detected += 1

        self.bus.emit(f"{self.name}_change", {
            "old": old_value,
            "new": new_value,
            "delta": (new_value - old_value) if isinstance(new_value, (int, float))
                     and isinstance(old_value, (int, float)) else None,
        }, source=source)
        return True


class EventDrivenComponent:
    """Base for event-driven Genesis components."""

    def __init__(self, name: str, bus: EventBus):
        self.name = name
        self.bus = bus
        self.active = False
        self.wake_count = 0
        self.idle_since = time.time()

    def wake(self, event_data: Dict[str, Any]) -> None:
        """Wake from idle on event."""
        self.active = True
        self.wake_count += 1
        self.process(event_data)
        self.active = False
        self.idle_since = time.time()

    def process(self, event_data: Dict[str, Any]) -> None:
        """Override: do actual work."""
        pass

    @property
    def idle_time_s(self) -> float:
        if self.active:
            return 0.0
        return round(time.time() - self.idle_since, 3)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "active": self.active,
            "wake_count": self.wake_count,
            "idle_time_s": self.idle_time_s,
        }


class EventDrivenSigma(EventDrivenComponent):
    """σ-measurement fires only when input changes."""

    def __init__(self, bus: EventBus):
        super().__init__("sigma_monitor", bus)
        self._last_sigma = 0
        self.sigma_detector = ChangeDetector("sigma", bus)
        bus.subscribe("input_change", self.wake)

    def process(self, event_data: Dict[str, Any]) -> None:
        text = event_data.get("text", "")
        sigma = check_output(text)
        self.sigma_detector.check(sigma, source=self.name)


class EventDrivenLivingWeights(EventDrivenComponent):
    """Living weights update only when σ changes."""

    def __init__(self, bus: EventBus):
        super().__init__("living_weights", bus)
        self.updates = 0
        bus.subscribe("sigma_change", self.wake)

    def process(self, event_data: Dict[str, Any]) -> None:
        self.updates += 1


class EventDrivenEpistemic(EventDrivenComponent):
    """Epistemic drive wakes only on new data."""

    def __init__(self, bus: EventBus):
        super().__init__("epistemic_drive", bus)
        self.discoveries = 0
        bus.subscribe("new_data", self.wake)
        bus.subscribe("sigma_change", self.wake)

    def process(self, event_data: Dict[str, Any]) -> None:
        self.discoveries += 1


class EventDrivenDashboard(EventDrivenComponent):
    """Dashboard refreshes only when there's something new."""

    def __init__(self, bus: EventBus):
        super().__init__("dashboard", bus)
        self.refreshes = 0
        bus.subscribe("sigma_change", self.wake)

    def process(self, event_data: Dict[str, Any]) -> None:
        self.refreshes += 1


class EventDrivenGenesis:
    """Fully event-driven Genesis system. Zero wasted compute."""

    def __init__(self) -> None:
        self.bus = EventBus()
        self.input_detector = ChangeDetector("input", self.bus)
        self.sigma_monitor = EventDrivenSigma(self.bus)
        self.living_weights = EventDrivenLivingWeights(self.bus)
        self.epistemic = EventDrivenEpistemic(self.bus)
        self.dashboard = EventDrivenDashboard(self.bus)
        self._components = [
            self.sigma_monitor, self.living_weights,
            self.epistemic, self.dashboard,
        ]

    def input(self, text: str) -> Dict[str, Any]:
        """Process input — fires events only on change."""
        changed = self.input_detector.check(text, source="user")
        if changed:
            self.bus.emit("input_change", {"text": text}, source="user")

        return {
            "input_changed": changed,
            "bus": self.bus.stats,
            "components": [c.to_dict() for c in self._components],
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "bus": self.bus.stats,
            "components": [c.to_dict() for c in self._components],
            "efficiency": {
                "events_emitted": self.bus.events_emitted,
                "events_suppressed": self.bus.events_suppressed,
                "suppression_ratio": (
                    round(self.bus.events_suppressed /
                          max(self.bus.events_emitted + self.bus.events_suppressed, 1), 3)
                ),
            },
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Event-Driven Genesis")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    if args.demo:
        edg = EventDrivenGenesis()
        print("=== Event-Driven Genesis Demo ===")

        r = edg.input("Hello world. 1 = 1.")
        print(f"Input 1: changed={r['input_changed']}")

        r = edg.input("Hello world. 1 = 1.")  # Same — suppressed
        print(f"Input 2 (same): changed={r['input_changed']}")

        r = edg.input("New input. Different text.")  # Changed — fires
        print(f"Input 3 (new): changed={r['input_changed']}")

        print(json.dumps(edg.stats, indent=2))
        print("\nZero wasted compute. 1 = 1.")
        return

    print("Event-Driven Genesis. 1 = 1.")


if __name__ == "__main__":
    main()

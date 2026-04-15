#!/usr/bin/env python3
"""
LOIKKA 40: σ-OPERATING SYSTEM — Genesis ON käyttöjärjestelmä.

Genesis is not an app running on an OS. Genesis IS the operating system.

Architecture:
  - Process manager: agent, twins, scientist, self-play run in parallel
  - Resource allocator: high-σ processes get more compute
  - Scheduler: epistemic drive in idle time, inference as priority
  - M4 P-core/E-core dispatch under kernel control
  - Daemon heartbeat (already exists) → expand to full OS scheduler

Usage:
    gos = GenesisOS()
    gos.spawn("scientist", topic="consciousness")
    gos.spawn("self_play", rounds=5)
    gos.scheduler.run()

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
import threading
from concurrent.futures import ThreadPoolExecutor, Future
from enum import IntEnum
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True

try:
    from genesis_executive import lagrangian, CognitionDepth
except ImportError:
    def lagrangian(s: int) -> float: return 1.0 - 2.0 * s


class ProcessPriority(IntEnum):
    IDLE = 0           # E-core: daemon, heartbeat
    BACKGROUND = 1     # E-core: epistemic drive, consolidation
    NORMAL = 2         # P-core: standard inference
    HIGH = 3           # P-core: high-σ processes need more compute
    CRITICAL = 4       # P-core exclusive: kernel enforcement


class GenesisProcess:
    """A managed process within Genesis OS."""

    _next_pid = 1
    _pid_lock = threading.Lock()

    def __init__(
        self,
        name: str,
        function: Callable[..., Any],
        priority: ProcessPriority = ProcessPriority.NORMAL,
        args: Optional[Dict[str, Any]] = None,
    ):
        with GenesisProcess._pid_lock:
            self.pid = GenesisProcess._next_pid
            GenesisProcess._next_pid += 1
        self.name = name
        self.function = function
        self.priority = priority
        self.args = args or {}
        self.state = "ready"
        self.result: Optional[Any] = None
        self.error: Optional[str] = None
        self.sigma: int = 0
        self.created_at = time.time()
        self.started_at: Optional[float] = None
        self.finished_at: Optional[float] = None
        self._future: Optional[Future] = None

    @property
    def elapsed_ms(self) -> Optional[float]:
        if self.started_at is None:
            return None
        end = self.finished_at or time.time()
        return round((end - self.started_at) * 1000)

    @property
    def core_affinity(self) -> str:
        if self.priority <= ProcessPriority.BACKGROUND:
            return "E-core"
        return "P-core"

    def to_dict(self) -> Dict[str, Any]:
        return {
            "pid": self.pid,
            "name": self.name,
            "priority": self.priority.name,
            "state": self.state,
            "sigma": self.sigma,
            "core_affinity": self.core_affinity,
            "elapsed_ms": self.elapsed_ms,
            "error": self.error,
        }


class ResourceAllocator:
    """Allocate compute resources based on σ."""

    def __init__(self, max_p_cores: int = 4, max_e_cores: int = 4) -> None:
        self.max_p_cores = max_p_cores
        self.max_e_cores = max_e_cores
        self._p_core_usage = 0
        self._e_core_usage = 0
        self._lock = threading.Lock()

    def allocate(self, process: GenesisProcess) -> Dict[str, Any]:
        """Allocate cores to a process based on priority and σ."""
        with self._lock:
            if process.core_affinity == "E-core":
                if self._e_core_usage < self.max_e_cores:
                    self._e_core_usage += 1
                    return {"allocated": True, "core": "E-core", "slot": self._e_core_usage}
            else:
                # High-σ processes get priority
                extra = 1 if process.sigma > 2 else 0
                needed = 1 + extra
                if self._p_core_usage + needed <= self.max_p_cores:
                    self._p_core_usage += needed
                    return {"allocated": True, "core": "P-core", "slots": needed}

        return {"allocated": False, "reason": "no cores available"}

    def release(self, process: GenesisProcess) -> None:
        with self._lock:
            if process.core_affinity == "E-core":
                self._e_core_usage = max(0, self._e_core_usage - 1)
            else:
                self._p_core_usage = max(0, self._p_core_usage - 1)

    @property
    def utilization(self) -> Dict[str, Any]:
        return {
            "p_cores": f"{self._p_core_usage}/{self.max_p_cores}",
            "e_cores": f"{self._e_core_usage}/{self.max_e_cores}",
            "p_util": round(self._p_core_usage / max(self.max_p_cores, 1), 2),
            "e_util": round(self._e_core_usage / max(self.max_e_cores, 1), 2),
        }


class Scheduler:
    """σ-aware process scheduler."""

    def __init__(self, allocator: ResourceAllocator) -> None:
        self.allocator = allocator
        self.ready_queue: List[GenesisProcess] = []
        self.running: List[GenesisProcess] = []
        self.completed: List[GenesisProcess] = []
        self._executor = ThreadPoolExecutor(max_workers=8, thread_name_prefix="genesis")

    def enqueue(self, process: GenesisProcess) -> None:
        self.ready_queue.append(process)
        self.ready_queue.sort(key=lambda p: p.priority, reverse=True)

    def tick(self) -> List[GenesisProcess]:
        """Run one scheduling cycle. Returns newly started processes."""
        started = []

        # Move finished processes
        still_running = []
        for proc in self.running:
            if proc._future and proc._future.done():
                try:
                    proc.result = proc._future.result()
                    proc.state = "completed"
                except Exception as e:
                    proc.error = str(e)
                    proc.state = "failed"
                proc.finished_at = time.time()
                self.allocator.release(proc)
                self.completed.append(proc)
            else:
                still_running.append(proc)
        self.running = still_running

        # Schedule ready processes
        ready_copy = list(self.ready_queue)
        self.ready_queue.clear()

        for proc in ready_copy:
            alloc = self.allocator.allocate(proc)
            if alloc["allocated"]:
                proc.state = "running"
                proc.started_at = time.time()
                proc._future = self._executor.submit(proc.function, **proc.args)
                self.running.append(proc)
                started.append(proc)
            else:
                self.ready_queue.append(proc)

        return started

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "ready": len(self.ready_queue),
            "running": len(self.running),
            "completed": len(self.completed),
            "utilization": self.allocator.utilization,
        }


# ── Built-in Genesis OS processes ─────────────────────────────────────────

def _proc_heartbeat(interval: float = 15.0, **kw: Any) -> Dict[str, Any]:
    """Daemon heartbeat process."""
    time.sleep(min(interval, 1.0))
    return {"heartbeat": True, "timestamp": time.time()}


def _proc_epistemic_drive(**kw: Any) -> Dict[str, Any]:
    """Epistemic drive: background knowledge consolidation."""
    try:
        from continual_learner import ContinualLearner
        learner = ContinualLearner()
        return learner.consolidate()
    except Exception as e:
        return {"status": "skipped", "reason": str(e)}


def _proc_self_play(rounds: int = 3, **kw: Any) -> Dict[str, Any]:
    """Self-play process."""
    return {"status": "self_play_stub", "rounds": rounds}


BUILTIN_PROCESSES: Dict[str, Tuple[Callable, ProcessPriority]] = {
    "heartbeat": (_proc_heartbeat, ProcessPriority.IDLE),
    "epistemic_drive": (_proc_epistemic_drive, ProcessPriority.BACKGROUND),
    "self_play": (_proc_self_play, ProcessPriority.NORMAL),
}


class GenesisOS:
    """σ-Operating System: manage all Genesis processes."""

    def __init__(self) -> None:
        self.allocator = ResourceAllocator()
        self.scheduler = Scheduler(self.allocator)
        self.boot_time = time.time()

    def spawn(
        self,
        name: str,
        priority: Optional[ProcessPriority] = None,
        function: Optional[Callable] = None,
        **kwargs: Any,
    ) -> GenesisProcess:
        """Spawn a new Genesis process."""
        if function is None and name in BUILTIN_PROCESSES:
            fn, default_prio = BUILTIN_PROCESSES[name]
            function = fn
            if priority is None:
                priority = default_prio

        if function is None:
            raise ValueError(f"Unknown process: {name}")
        if priority is None:
            priority = ProcessPriority.NORMAL

        proc = GenesisProcess(name=name, function=function, priority=priority, args=kwargs)
        self.scheduler.enqueue(proc)
        return proc

    def tick(self) -> Dict[str, Any]:
        """Run one OS scheduling cycle."""
        started = self.scheduler.tick()
        return {
            "started": [p.to_dict() for p in started],
            "scheduler": self.scheduler.stats,
        }

    def run(self, ticks: int = 10, tick_interval: float = 0.1) -> Dict[str, Any]:
        """Run the OS for N ticks."""
        results = []
        for i in range(ticks):
            r = self.tick()
            results.append(r)
            if not self.scheduler.ready_queue and not self.scheduler.running:
                break
            time.sleep(tick_interval)
        return {
            "ticks": len(results),
            "final_stats": self.scheduler.stats,
            "completed": [p.to_dict() for p in self.scheduler.completed],
        }

    @property
    def uptime_s(self) -> float:
        return round(time.time() - self.boot_time, 1)

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "uptime_s": self.uptime_s,
            "scheduler": self.scheduler.stats,
            "utilization": self.allocator.utilization,
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Genesis OS")
    ap.add_argument("--spawn", nargs="+", help="Spawn processes")
    ap.add_argument("--ticks", type=int, default=10)
    ap.add_argument("--demo", action="store_true", help="Run demo with builtin processes")
    args = ap.parse_args()

    gos = GenesisOS()

    if args.demo:
        gos.spawn("heartbeat")
        gos.spawn("epistemic_drive")
        result = gos.run(ticks=args.ticks)
        print(json.dumps(result, indent=2, default=str))
        print(f"\nUptime: {gos.uptime_s}s")
        print("1 = 1.")
        return

    if args.spawn:
        for name in args.spawn:
            gos.spawn(name)
        result = gos.run(ticks=args.ticks)
        print(json.dumps(result, indent=2, default=str))
        return

    print(json.dumps(gos.stats, indent=2))
    print("1 = 1.")


if __name__ == "__main__":
    main()

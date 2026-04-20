# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of the Atlantean Codex loader (`src/sigma/pipeline/codex.c`).

Keeps byte-identical semantics with the C primitive:

    * FNV-1a-64 over the raw file bytes.
    * Chapter count = number of lines starting with ``CHAPTER ``.
    * Invariant check = substring search for ``1 = 1``.

This module is imported by the Python-side ``sigma_pipeline`` test
harness and by ``scripts/sigma_pipeline/orchestrator.py`` to seed the
engram with the Codex before any user turn is processed.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Optional

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK_64 = (1 << 64) - 1

CODEX_FULL_PATH = Path("data/codex/atlantean_codex.txt")
CODEX_SEED_PATH = Path("data/codex/codex_seed.txt")
MIN_CHAPTERS_FULL = 33


def fnv1a_64(buf: bytes) -> int:
    h = FNV_OFFSET
    for b in buf:
        h ^= b
        h = (h * FNV_PRIME) & MASK_64
    return h


def count_chapters(text: str) -> int:
    n = 0
    if text.startswith("CHAPTER "):
        n += 1
    n += sum(1 for line in text.split("\n")[1:] if line.startswith("CHAPTER "))
    return n


@dataclass
class Codex:
    bytes_: bytes
    size: int
    hash_fnv1a64: int
    chapters_found: int
    is_seed: bool

    @property
    def contains_invariant(self) -> bool:
        return b"1 = 1" in self.bytes_


def load(path: Optional[Path | str] = None) -> Codex:
    """Load the Codex from `path` (defaults to the full canonical file)."""
    p = Path(path) if path is not None else CODEX_FULL_PATH
    data = p.read_bytes()
    return Codex(
        bytes_=data,
        size=len(data),
        hash_fnv1a64=fnv1a_64(data),
        chapters_found=count_chapters(data.decode("utf-8", errors="replace")),
        is_seed=("codex_seed" in str(p)),
    )


def load_seed() -> Codex:
    c = load(CODEX_SEED_PATH)
    return Codex(
        bytes_=c.bytes_,
        size=c.size,
        hash_fnv1a64=c.hash_fnv1a64,
        chapters_found=c.chapters_found,
        is_seed=True,
    )


__all__ = [
    "Codex",
    "load",
    "load_seed",
    "fnv1a_64",
    "count_chapters",
    "CODEX_FULL_PATH",
    "CODEX_SEED_PATH",
    "MIN_CHAPTERS_FULL",
]

# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Python mirror of ``src/sigma/pipeline/multimodal.{h,c}``.

Byte-for-byte decision parity with the C primitive on the canonical
rows (text / code / schema / image / audio).  Python adds one thing
the C primitive deliberately doesn't: a full ``json.loads``-based
schema validator that checks not just presence but TYPES of required
fields.  Callers that want the strict-equivalence-with-C path should
use ``measure_schema_cheap``; callers that want full validation use
``measure_schema``.
"""

from __future__ import annotations

import enum
import json
import math
import re
from typing import Any, Dict, Iterable, Mapping, Optional, Sequence


class Modality(enum.Enum):
    TEXT       = 0
    CODE       = 1
    STRUCTURED = 2
    IMAGE      = 3
    AUDIO      = 4


def _clamp01(x: float) -> float:
    try:
        v = float(x)
    except (TypeError, ValueError):
        return 1.0
    if math.isnan(v):
        return 1.0
    if v < 0.0:
        return 0.0
    if v > 1.0:
        return 1.0
    return v


def measure_text(sigma_in: float) -> float:
    return _clamp01(sigma_in)


def measure_code(src: str) -> float:
    """Balance check over () [] {} plus " and ' quote parity.

    Semantics match the C implementation.  Backslash escapes one
    character inside either quote form.
    """
    if src is None:
        return 1.0
    p = b = c = 0
    in_dq = False
    in_sq = False
    i = 0
    n = len(src)
    while i < n:
        ch = src[i]
        if ch == "\\" and (in_dq or in_sq):
            i += 2
            continue
        if in_dq:
            if ch == '"':
                in_dq = False
            i += 1
            continue
        if in_sq:
            if ch == "'":
                in_sq = False
            i += 1
            continue
        if ch == '"':   in_dq = True
        elif ch == "'": in_sq = True
        elif ch == "(": p += 1
        elif ch == ")": p -= 1
        elif ch == "[": b += 1
        elif ch == "]": b -= 1
        elif ch == "{": c += 1
        elif ch == "}": c -= 1
        if p < 0 or b < 0 or c < 0:
            return 1.0
        i += 1
    if p != 0 or b != 0 or c != 0 or in_dq or in_sq:
        return 1.0
    return 0.10


_FIELD_RE = re.compile(r'"([^"\\]+)"\s*:')


def measure_schema_cheap(blob: str,
                         required: Optional[Sequence[str]]) -> float:
    """Bytes-identical-to-C σ-schema check (substring scan)."""
    if blob is None:
        return 1.0
    if not required:
        return 0.10
    missing = 0
    for field in required:
        pat = f'"{field}":'
        idx = 0
        found = False
        while True:
            hit = blob.find(f'"{field}"', idx)
            if hit < 0:
                break
            j = hit + len(field) + 2
            while j < len(blob) and blob[j] in " \t\n\r":
                j += 1
            if j < len(blob) and blob[j] == ":":
                found = True
                break
            idx = hit + 1
        if not found:
            missing += 1
    frac = missing / len(required)
    return _clamp01(0.10 + 0.90 * frac)


def measure_schema(blob: str,
                   schema: Mapping[str, Any]) -> float:
    """Full-fat schema validator using json.loads + type checks.

    ``schema`` maps field name → expected Python type (e.g. str, int,
    dict, list).  For each required field that is missing OR whose
    JSON type doesn't match, σ gets a 1/n boost.  Parse failure →
    σ = 1.0.  Pure-Python, no jsonschema dep.
    """
    if blob is None:
        return 1.0
    try:
        obj = json.loads(blob)
    except (ValueError, TypeError):
        return 1.0
    if not isinstance(obj, dict):
        return 1.0
    if not schema:
        return 0.10
    miss = 0
    for k, t in schema.items():
        if k not in obj or not isinstance(obj[k], t):
            miss += 1
    frac = miss / len(schema)
    return _clamp01(0.10 + 0.90 * frac)


def measure_image(similarity: float) -> float:
    try:
        s = float(similarity)
    except (TypeError, ValueError):
        return 1.0
    if math.isnan(s):
        return 1.0
    return _clamp01(1.0 - s)


def measure_audio(similarity: float) -> float:
    return measure_image(similarity)


def measure_multimodal(m: Modality, payload: Any,
                       required: Optional[Sequence[str]] = None
                       ) -> float:
    if m == Modality.TEXT:
        return measure_text(float(payload))
    if m == Modality.CODE:
        return measure_code(str(payload))
    if m == Modality.STRUCTURED:
        return measure_schema_cheap(str(payload), required)
    if m == Modality.IMAGE:
        return measure_image(float(payload))
    if m == Modality.AUDIO:
        return measure_audio(float(payload))
    return 1.0

# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""OpenAI-SDK-shape compatibility helpers for Creation OS.

The v106 HTTP kernel already speaks a subset of the OpenAI
Chat Completions API.  This module provides:

* ``sample_openai_response()`` — a canonical response fixture used
  by the v142 merge-gate and by downstream integrations to verify
  that :class:`creation_os.COS` correctly parses the shape.
* ``sample_openai_headers()`` — the X-COS-* headers v106 emits.
* ``check_openai_drop_in()`` — returns True if the installed
  ``openai`` package can construct a client against Creation OS
  without errors.  Returns False (not raises) when ``openai`` is
  missing so CI environments without the SDK still pass.
"""
from __future__ import annotations

from typing import Any, Dict, Tuple


def sample_openai_response() -> Dict[str, Any]:
    """Return a canonical OpenAI-compat response body."""
    return {
        "id": "chatcmpl-cos-0001",
        "object": "chat.completion",
        "created": 1_734_000_000,
        "model": "creation-os/bitnet-2b",
        "choices": [
            {
                "index": 0,
                "message": {
                    "role": "assistant",
                    "content": "Hello from Creation OS.",
                },
                "finish_reason": "stop",
            }
        ],
        "usage": {
            "prompt_tokens": 12,
            "completion_tokens": 7,
            "total_tokens": 19,
        },
    }


def sample_openai_headers() -> Dict[str, str]:
    """Return canonical X-COS-* headers emitted by v106."""
    return {
        "Content-Type": "application/json",
        "X-COS-Sigma-Product": "0.231847",
        "X-COS-Sigma-Threshold": "0.500000",
        "X-COS-Specialist": "bitnet-2b-reasoning",
        "X-COS-Verdict": "emit",
    }


def check_openai_drop_in(base_url: str = "http://localhost:8080/v1") -> Tuple[bool, str]:
    """Attempt to construct an ``openai.OpenAI`` client.

    Returns (ok, reason).  ``ok=False`` when the ``openai`` package
    is not installed, which is *not* a failure — the Python SDK
    itself has no hard dependency on openai.
    """
    try:
        from openai import OpenAI  # type: ignore
    except ImportError as e:
        return False, f"openai sdk not installed ({e.__class__.__name__}) — drop-in skipped"
    try:
        _ = OpenAI(base_url=base_url, api_key="cos")
        return True, "openai.OpenAI constructs against Creation OS base_url"
    except Exception as e:  # noqa: BLE001
        return False, f"openai.OpenAI construct failed: {e!r}"

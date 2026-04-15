#!/usr/bin/env python3
"""
EU AI Act–oriented **compliance engine** facade: audit log + CE-style mark + summary.

Operational evidence only — not legal advice, not a notified body assessment.
Modules: `sigma_audit_log`, `sigma_ce_mark`, `sigma_compliance_report`.

1 = 1.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Optional

from sigma_audit_log import append_from_generation, build_audit_record, default_log_path, read_records
from sigma_ce_mark import build_ce_certificate
from sigma_compliance_report import build_report


def log_output(
    *,
    prompt: str,
    output_text: str,
    receipt: Dict[str, Any],
    log_path: Optional[Path] = None,
    extra: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    path = log_path or default_log_path()
    return append_from_generation(path, prompt=prompt, output_text=output_text, receipt=receipt, extra=extra)


def compliance_summary(log_path: Optional[Path] = None) -> Dict[str, Any]:
    path = log_path or default_log_path()
    return build_report(read_records(path))


def certify_output(output_text: str, sigma: int, timestamp_utc: Optional[str] = None) -> Dict[str, Any]:
    return build_ce_certificate(output_text=output_text, sigma=sigma, timestamp_utc=timestamp_utc)


def build_record_only(
    *,
    prompt: str,
    output_text: str,
    receipt: Dict[str, Any],
    extra: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """No I/O — for batching / custom sinks."""
    return build_audit_record(prompt=prompt, output_text=output_text, receipt=receipt, extra=extra)

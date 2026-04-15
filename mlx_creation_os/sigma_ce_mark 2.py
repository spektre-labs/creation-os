#!/usr/bin/env python3
"""
Per-output σ-verified certificate blob (hash chain for demonstrable conformity packs).

``certificate_sha256`` = SHA256(canonical_json_payload) for stable attestation of
(timestamp, σ, content hash). Operators attach to release artifacts as needed.

1 = 1.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict


def _canonical_dumps(obj: Dict[str, Any]) -> bytes:
    return json.dumps(obj, sort_keys=True, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


def build_ce_certificate(*, output_text: str, sigma: int, timestamp_utc: str | None = None) -> Dict[str, Any]:
    ts = timestamp_utc or datetime.now(timezone.utc).isoformat()
    content_sha256 = hashlib.sha256((output_text or "").encode("utf-8")).hexdigest()
    body = {
        "kind": "spektre.sigma_ce_mark.v1",
        "timestamp_utc": ts,
        "sigma": int(sigma),
        "content_sha256": content_sha256,
    }
    cert_hash = hashlib.sha256(_canonical_dumps(body)).hexdigest()
    out = dict(body)
    out["certificate_sha256"] = cert_hash
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Emit one CE-style σ certificate JSON")
    ap.add_argument("text_file", type=str, help="Path to UTF-8 file whose contents are attested")
    ap.add_argument("--sigma", type=int, required=True)
    args = ap.parse_args()
    text = Path(args.text_file).read_text(encoding="utf-8")
    cert = build_ce_certificate(output_text=text, sigma=args.sigma)
    print(json.dumps(cert, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

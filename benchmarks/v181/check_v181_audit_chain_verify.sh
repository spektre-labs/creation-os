#!/usr/bin/env bash
#
# v181 σ-Audit — merge-gate check.
#
# Contracts:
#   * self-test verifies a 1 000-entry chain;
#   * summary JSON reports n_entries == 1000;
#   * anomaly_detected == true and rel_rise_pct ≥ 30;
#   * JSONL export contains one hash/sig per entry;
#   * output is byte-deterministic.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v181_audit"
[ -x "$BIN" ] || { echo "v181: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, sys
d = json.loads("""$OUT""")
assert d["n_entries"] == 1000, f"n_entries={d['n_entries']}"
assert d["anomaly_detected"] is True, f"anomaly not detected: {d}"
assert d["anomaly_rel_rise_pct"] >= 30.0, \
    f"rel_rise={d['anomaly_rel_rise_pct']} < 30"
PY

LINES=$("$BIN" --export | wc -l | awk '{print $1}')
if [ "$LINES" -ne 1000 ]; then
    echo "v181: expected 1000 JSONL lines, got $LINES" >&2
    exit 1
fi

# Spot-check a middle line parses and contains required fields.
LINE=$("$BIN" --export | sed -n '500p')
python3 - <<PY
import json
e = json.loads("""$LINE""")
for k in ("id","ts","sigma","decision","input_hash","output_hash",
          "prev_hash","self_hash","sig"):
    assert k in e, f"missing field: {k}"
assert len(e["input_hash"])  == 64, "input_hash not 32 B hex"
assert len(e["output_hash"]) == 64, "output_hash not 32 B hex"
assert len(e["prev_hash"])   == 64, "prev_hash not 32 B hex"
assert len(e["self_hash"])   == 64, "self_hash not 32 B hex"
assert len(e["sig"])         == 64, "sig not 32 B hex"
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v181: non-deterministic summary" >&2
    exit 1
fi

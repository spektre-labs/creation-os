#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Merge-gate for v132 σ-Persona.
#
# Validates:
#   1. self-test green (expertise + feedback + TOML round-trip)
#   2. --adapt: 10 low-σ math observations → expert (or advanced with
#      EMA prior); --adapt: 10 high-σ biology observations → beginner
#   3. --feedback: "too-long" changes style.length from normal → concise
#   4. --demo: emits valid JSON + TOML shape
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN_SRC="$ROOT/creation_os_v132_persona"
if [ ! -x "$BIN_SRC" ]; then
    echo "check-v132: building creation_os_v132_persona..." >&2
    make -s creation_os_v132_persona
fi

# Run from a temp path: on some hosts (e.g. Desktop synced by File Provider),
# dyld can stall indefinitely when executing the same bytes from the workspace
# path; copying to /tmp avoids the hang while preserving identical behavior.
run_v132() {
    local tmp ec
    tmp="$(mktemp /tmp/cos_v132_persona_XXXXXX)"
    cp "$BIN_SRC" "$tmp"
    chmod +x "$tmp"
    "$tmp" "$@"
    ec=$?
    rm -f "$tmp"
    return "$ec"
}

echo "check-v132-persona-adaptation: --self-test"
run_v132 --self-test >/dev/null

echo "check-v132-persona-adaptation: 10 low-σ math observations"
OUT_M="$(run_v132 --adapt math 0.10 10)"
echo "  $OUT_M"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$OUT_M""").group(0))
assert d["topic"]   == "math"
assert d["samples"] == 10
# With EMA starting at 0.50 and α=0.3, 10 updates of 0.10 converge
# to ≈0.12 → expert.  Accept either expert or advanced.
assert d["level"] in ("expert", "advanced"), f"unexpected level {d['level']}"
assert d["ema_sigma"] < 0.25, f"ema_sigma={d['ema_sigma']} too high"
print(f"  math ema={d['ema_sigma']:.4f} level={d['level']} ok")
EOF

echo "check-v132-persona-adaptation: 10 high-σ biology observations"
OUT_B="$(run_v132 --adapt biology 0.85 10)"
echo "  $OUT_B"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$OUT_B""").group(0))
assert d["topic"]   == "biology"
assert d["samples"] == 10
assert d["level"]   == "beginner", f"unexpected level {d['level']}"
assert d["ema_sigma"] > 0.60, f"ema_sigma={d['ema_sigma']} too low"
print(f"  biology ema={d['ema_sigma']:.4f} level={d['level']} ok")
EOF

echo "check-v132-persona-adaptation: feedback too-long → concise"
OUT_F="$(run_v132 --feedback too-long)"
echo "  $OUT_F"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$OUT_F""").group(0))
assert d["applied"] == 1
assert d["length"]  == "concise", f"length={d['length']} (expected concise)"
print(f"  too-long → length={d['length']} ok")
EOF

echo "check-v132-persona-adaptation: demo emits JSON + TOML"
OUT_D="$(run_v132 --demo)"
echo "$OUT_D" | head -5
python3 - <<EOF
import re, json
out = """$OUT_D"""
# First line: JSON
first = out.splitlines()[0]
d = json.loads(first)
assert "style" in d and "topics" in d
# TOML section starts after '---'
assert "[persona]" in out
assert "[style]"   in out
assert "[expertise]" in out
assert "math"    in out
assert "biology" in out
print("  demo JSON + TOML shape ok")
EOF

echo "check-v132-persona-adaptation: OK"

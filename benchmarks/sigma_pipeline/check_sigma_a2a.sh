#!/usr/bin/env bash
#
# OMEGA-2 smoke test: σ-A2A agent-to-agent with σ-trust.
#
# Pinned facts (per a2a_main.c):
#   self_test == 0
#   self_id == "creation-os-sigma"
#   tau_block == 0.9, lr == 0.2
#   self_card advertises a2a/1.0 + sigma_gate=true + scsl_compliant
#   Three peers after 20 rounds:
#     peer-good    sigma_trust < 0.25, blocklisted=false
#     peer-drift   0.25 < sigma_trust < 0.80, blocklisted=false
#     peer-hostile sigma_trust > 0.90, blocklisted=true, blocked>0
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_a2a"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_a2a: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_a2a", doc
assert doc["self_test"] == 0, doc
assert doc["self_id"] == "creation-os-sigma", doc
assert abs(doc["tau_block"] - 0.9) < 1e-4, doc
assert abs(doc["lr"] - 0.2) < 1e-4, doc

card = doc["self_card"]
assert card["protocol"] == "a2a/1.0", card
assert card["sigma_gate"] is True, card
assert card["scsl_compliant"] is True, card
assert "sigma_measurement" in card["capabilities"], card

peers = {p["agent_id"]: p for p in doc["peers"]}
assert set(peers) == {"peer-good", "peer-drift", "peer-hostile"}, peers

good = peers["peer-good"]
assert good["sigma_trust"] < 0.25, good
assert good["blocklisted"] is False, good
assert good["exchanges_total"] == 20, good

drift = peers["peer-drift"]
assert 0.25 < drift["sigma_trust"] < 0.80, drift
assert drift["blocklisted"] is False, drift

hostile = peers["peer-hostile"]
assert hostile["sigma_trust"] > doc["tau_block"], hostile
assert hostile["blocklisted"] is True, hostile
assert hostile["exchanges_blocked"] > 0, hostile

print("check_sigma_a2a: PASS", {
    "good_trust":    good["sigma_trust"],
    "drift_trust":   drift["sigma_trust"],
    "hostile_trust": hostile["sigma_trust"],
    "blocklisted":   hostile["blocklisted"],
})
PY

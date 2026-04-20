#!/usr/bin/env bash
#
# OMEGA-4 smoke test: σ-mesh3 three-node live mesh simulation.
#
# Pinned facts (per mesh3_main.c):
#   self_test == 0
#   tau_escalate == 0.55, dp_noise_sigma == 0.05
#   nodes: A(coordinator), B(leaf), C(federator)
#   Canonical trace for a 3-query batch + one tamper probe:
#     Q1 "Who wrote Tähtien sota?"        → B answers locally
#     Q2 "Mikä on σ?"                     → B escalates → C → A
#     Q3 "Selitä Finnish coffee culture." → B escalates → C → A
#     Each query emits FEDERATION to {A,B} → 6 federation messages
#     summary.escalations == 2
#     summary.federations == 6
#     summary.signatures_rejected == 1 (tamper probe)
#     summary.tamper_probe == 1 (rejected as expected)
#     Every non-tampered message has verified=true, dropped=false
#   All final_sigmas ≤ 0.50 (either leaf answer or authoritative
#   answer from C; leaf sigma-high cases escalate so never returned)
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_mesh3"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_mesh3: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_mesh3", doc
assert doc["self_test"] == 0, doc
assert abs(doc["tau_escalate"] - 0.55) < 1e-4, doc
assert abs(doc["dp_noise_sigma"] - 0.05) < 1e-4, doc

nodes = {n["name"]: n for n in doc["nodes"]}
assert set(nodes) == {"A-helsinki-m3", "B-kerava-rpi5", "C-cloud-vm"}, nodes
assert nodes["A-helsinki-m3"]["role"] == "coordinator"
assert nodes["B-kerava-rpi5"]["role"] == "leaf"
assert nodes["C-cloud-vm"]["role"]   == "federator"

# B sees all three queries, C sees the two escalated ones.
assert nodes["B-kerava-rpi5"]["queries_seen"] == 3, nodes
assert nodes["C-cloud-vm"]["queries_seen"]   == 2, nodes
# A and B both receive federation deltas per query (3 each).
assert nodes["A-helsinki-m3"]["federation_updates_applied"] == 3, nodes
assert nodes["B-kerava-rpi5"]["federation_updates_applied"] == 3, nodes

summ = doc["summary"]
assert summ["escalations"]         == 2, summ
assert summ["federations"]         == 6, summ
assert summ["signatures_rejected"] == 1, summ
assert summ["tamper_probe"]        == 1, summ

# Message log sanity: every non-dropped message must verify.
for m in doc["messages"]:
    if m["dropped"]:
        assert m["verified"] is False, m
    else:
        assert m["verified"] is True, m

# Final σ returned to A is ≤ 0.5 for every query.
for s in doc["final_sigmas"]:
    assert 0.0 <= s <= 0.50, doc["final_sigmas"]

print("check_sigma_mesh3: PASS", {
    "messages":     summ["messages"],
    "escalations":  summ["escalations"],
    "federations":  summ["federations"],
    "tamper_block": summ["tamper_probe"] == 1,
})
PY

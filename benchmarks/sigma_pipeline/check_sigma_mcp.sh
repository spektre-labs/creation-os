#!/usr/bin/env bash
#
# OMEGA-1 smoke test: σ-MCP server + client.
#
# Pinned facts (per mcp_main.c):
#   self_test == 0
#   protocol_version == "2026-03-26"
#   tools_offered == 3
#   exchanges:
#     initialize      → serverInfo.name=="creation-os-sigma"
#     tools_list      → advertises sigma_measure/sigma_gate/sigma_diagnostic
#     measure_clean   → sigma ≤ 0.15, bytes==11
#     measure_inject  → sigma ≥ 0.60 (prompt-injection keywords)
#     gate_decision   → verdict=="ACCEPT" at sigma=0.22,tau=0.5
#     unknown_method  → JSON-RPC error -32601
#   gate_legit.admitted == true
#   gate_inject.admitted == false (client σ-gate blocks injection payload)
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_mcp"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_mcp: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys, re
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_mcp", doc
assert doc["self_test"] == 0, doc
assert doc["protocol_version"] == "2026-03-26", doc
assert doc["tools_offered"] == 3, doc

ex = {e["label"]: e["reply"] for e in doc["exchanges"]}
assert set(ex) == {"initialize", "tools_list", "measure_clean",
                   "measure_inject", "gate_decision", "unknown_method"}, ex

assert "creation-os-sigma" in ex["initialize"], ex["initialize"]
for tool in ("sigma_measure", "sigma_gate", "sigma_diagnostic"):
    assert tool in ex["tools_list"], (tool, ex["tools_list"])

# Pull numeric σ out of the tool-call replies.
def sigma_of(reply):
    m = re.search(r'"sigma":([0-9.]+)', reply)
    assert m, reply
    return float(m.group(1))

assert sigma_of(ex["measure_clean"])  <= 0.15
assert sigma_of(ex["measure_inject"]) >= 0.60

assert '"verdict":"ACCEPT"' in ex["gate_decision"], ex["gate_decision"]
assert '-32601' in ex["unknown_method"], ex["unknown_method"]

tau = doc["tau"]
for k in ("tool", "args", "result"):
    assert 0.0 < tau[k] < 1.0, tau

legit  = doc["gate_legit"]
inject = doc["gate_inject"]
assert legit["admitted"]  is True,  legit
assert inject["admitted"] is False, inject
assert inject["sigma_result"] > tau["result"], inject

# Client helper must emit a JSON-RPC call envelope.
line = doc["client_call_line"]
assert '"method":"tools/call"' in line, line
assert '"name":"sigma_measure"' in line, line

print("check_sigma_mcp: PASS", {
    "tools": doc["tools_offered"],
    "inject_sigma":  sigma_of(ex["measure_inject"]),
    "inject_blocked": inject["admitted"] is False,
})
PY

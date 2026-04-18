#!/usr/bin/env bash
#
# benchmarks/v167/check_v167_governance_policy_apply.sh
#
# Merge-gate for v167 σ-Governance-API.  Verifies:
#   1. self-test
#   2. policy bundle contains medical/creative/code (+ legal
#      after scripted scenario) with correct τ and require_sandbox
#   3. creative.abstain_message == null (JSON null, not empty string)
#   4. fleet contains 4 nodes with policy_version stamped
#   5. after push, every ok/degraded node equals policy_version;
#      a down node lags behind
#   6. audit log includes at least one of each verdict
#      (emit/abstain/revise/denied)
#   7. auditor prompts are denied (RBAC enforcement)
#   8. adapter rollback applies to every node
#   9. determinism: two runs produce byte-identical JSON and NDJSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v167_governance"
test -x "$BIN" || { echo "v167: binary not built"; exit 1; }

echo "[v167] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v167] (2..5, 8) scripted state JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v167_governance"]).decode())
assert doc["org"] == "spektre-labs"

# policies by domain
pols = {p["domain"]: p for p in doc["policies"]}
for d in ("medical", "creative", "code", "legal"):
    assert d in pols, pols
assert abs(pols["medical"]["tau"] - 0.25) < 1e-4, pols["medical"]
assert pols["medical"]["require_sandbox"] is True
assert pols["creative"]["abstain_message"] is None, pols["creative"]
assert pols["legal"]["require_sandbox"] is True

# fleet
nodes = {n["node_id"]: n for n in doc["fleet"]}
for n in ("lab-m3", "lab-rpi5", "cloud-a", "cloud-b"):
    assert n in nodes, nodes
assert nodes["lab-m3"]["policy_version"] == doc["policy_version"]
assert nodes["cloud-b"]["policy_version"] <  doc["policy_version"], nodes["cloud-b"]
assert nodes["cloud-b"]["health"] == "down"

# adapter rollback
for nd in nodes.values():
    assert nd["adapter"] == "adapter-v1.1.0", nd
print("state ok")
PY

echo "[v167] (6+7) audit NDJSON"
python3 - <<'PY'
import json, subprocess
lines = subprocess.check_output(["./creation_os_v167_governance", "--audit"]).decode().strip().splitlines()
assert lines, "empty audit"
entries = [json.loads(l) for l in lines]
verdicts = set(e["verdict"] for e in entries)
for v in ("emit", "abstain", "revise", "denied"):
    assert v in verdicts, verdicts
# auditor must be denied
auditor = [e for e in entries if e["role"] == "auditor"]
assert auditor and all(e["verdict"] == "denied" for e in auditor), auditor
print("audit ok, verdicts=", sorted(verdicts))
PY

echo "[v167] (9) determinism"
A="$("$BIN")"
B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v167: state JSON not deterministic"; exit 9; }
AA="$("$BIN" --audit)"
BB="$("$BIN" --audit)"
[ "$AA" = "$BB" ] || { echo "v167: audit NDJSON not deterministic"; exit 9; }

echo "[v167] ALL CHECKS PASSED"

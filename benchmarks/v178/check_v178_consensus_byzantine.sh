#!/usr/bin/env bash
#
# benchmarks/v178/check_v178_consensus_byzantine.sh
#
# Merge-gate for v178 σ-Consensus.  Verifies:
#   1. self-test
#   2. 5 nodes (incl. 1 byzantine, 1 young); 4 claims with
#      decisions accept=2 / reject=1 / abstain=1
#   3. byzantine node's reputation ended below its start
#      value (decayed to ≤ 0.1)
#   4. all mature honest nodes gained reputation
#   5. young node voted with majority at least twice →
#      reputation net-positive
#   6. merkle_root is 64 hex chars and matches --root output
#   7. determinism
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v178_consensus"
test -x "$BIN" || { echo "v178: binary not built"; exit 1; }

echo "[v178] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v178] (2..5) full JSON"
python3 - <<'PY'
import json, subprocess, re
doc = json.loads(subprocess.check_output(["./creation_os_v178_consensus"]).decode())
assert doc["n_nodes"] == 5
assert doc["n_claims"] == 4
assert doc["n_accept"] == 2, doc
assert doc["n_reject"] == 1, doc
assert doc["n_abstain"] == 1, doc

nodes = {n["name"]: n for n in doc["nodes"]}
byz = nodes["N4-byz"]
assert byz["byzantine"] is True
assert byz["reputation"] <= 0.1, byz
for name in ("N0-alpha", "N1-beta", "N2-gamma"):
    assert nodes[name]["reputation"] > 3.0, nodes[name]
young = nodes["N3-young"]
assert young["reputation"] > 1.0, young   # net positive
assert young["n_correct"] >= 2, young

root = doc["merkle_root"]
assert re.fullmatch(r"[0-9a-f]{64}", root), root
print("consensus ok:", doc["n_accept"], doc["n_reject"], doc["n_abstain"],
      "root", root[:16] + "...")
PY

echo "[v178] (6) --root matches"
ROOT_CLI="$("$BIN" --root)"
ROOT_JSON="$("$BIN" | python3 -c "import json,sys; print(json.loads(sys.stdin.read())['merkle_root'])")"
[ "$ROOT_CLI" = "$ROOT_JSON" ] || { echo "root mismatch"; exit 6; }

echo "[v178] (7) determinism"
A="$("$BIN")";       B="$("$BIN")";       [ "$A" = "$B" ] || { echo "json nondet"; exit 7; }
A="$("$BIN" --root)"; B="$("$BIN" --root)"; [ "$A" = "$B" ] || { echo "root nondet"; exit 7; }

echo "[v178] ALL CHECKS PASSED"

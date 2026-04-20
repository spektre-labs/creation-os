#!/usr/bin/env bash
#
# FINAL-3 smoke test: σ-Offline — airplane-mode readiness.
#
# Pinned facts (per offline_main.c):
#   self_test_rc                          == 0
#   full.verdict                          == "ready"
#   full.required                         == 4
#   full.required_ok                      == 4
#   full.total                            == 5       (model, engram, rag,
#                                                      codex, persona)
#   full.ok                                == 5
#   full.probes[*].status == "ok" for all 5
#   full.probes[*].hash stable across two invocations (content-based)
#   after_rm_rag.verdict                  == "not_ready"
#   after_rm_rag.required_ok              == 3
#   after_rm_rag.probes[rag].status       == "missing"
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_offline"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"

python3 - "$OUT1" "$OUT2" <<'PY'
import json, sys
with open(sys.argv[1]) as f: d1 = json.load(f)
with open(sys.argv[2]) as f: d2 = json.load(f)

assert d1["kernel"] == "sigma_offline", d1
assert d1["self_test"] == 0, d1

full = d1["full"]
assert full["verdict"] == "ready", full
assert full["required"] == 4, full
assert full["required_ok"] == 4, full
assert full["total"] == 5, full
assert full["ok"] == 5, full

names = [p["name"] for p in full["probes"]]
assert names == ["model", "engram", "rag", "codex", "persona"], names
for p in full["probes"]:
    assert p["status"] == "ok", p
    assert p["present"] is True, p
    assert p["size"] > 0, p
    assert len(p["hash"]) == 16, p

deg = d1["after_rm_rag"]
assert deg["verdict"] == "not_ready", deg
assert deg["required_ok"] == 3, deg
rag = next(p for p in deg["probes"] if p["name"] == "rag")
assert rag["status"] == "missing", rag
assert rag["present"] is False, rag

# Determinism: hashes are content-based so two runs must agree
# even though the containing directory name changes per-PID.
h1 = {p["name"]: p["hash"] for p in d1["full"]["probes"]}
h2 = {p["name"]: p["hash"] for p in d2["full"]["probes"]}
assert h1 == h2, (h1, h2)

print("check_sigma_offline: PASS", {
    "verdict_full": full["verdict"],
    "verdict_degraded": deg["verdict"],
    "required_ok": full["required_ok"],
})
PY

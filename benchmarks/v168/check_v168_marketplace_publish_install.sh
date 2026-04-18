#!/usr/bin/env bash
#
# benchmarks/v168/check_v168_marketplace_publish_install.sh
#
# Merge-gate for v168 σ-Marketplace.  Verifies:
#   1. self-test
#   2. registry lists 6 artifacts, at least one of each kind
#   3. deterministic 16-hex sha per artifact (FNV demo)
#   4. healthy artifact (σ < 0.15) installs with status=ok
#   5. high-σ artifact (σ > 0.50) refused without --force
#   6. high-σ artifact installed with --force → status=forced
#   7. search filters by substring (term "skill" matches ≥ 2)
#   8. .cos-skill fixture validator accepts full set, rejects
#      a missing README
#   9. determinism: two registry listings byte-identical
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v168_marketplace"
test -x "$BIN" || { echo "v168: binary not built"; exit 1; }

echo "[v168] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v168] (2+3) registry contents"
python3 - <<'PY'
import json, re, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v168_marketplace"]).decode())
assert doc["n_artifacts"] == 6, doc
kinds = set(a["kind"] for a in doc["artifacts"])
for k in ("skill", "kernel", "plugin"):
    assert k in kinds, kinds
for a in doc["artifacts"]:
    assert re.fullmatch(r"[0-9a-f]{16}", a["sha"]), a
    assert 0.0 <= a["sigma_reputation"] <= 1.0, a
print("registry ok, kinds=", sorted(kinds))
PY

echo "[v168] (4) healthy install"
OK="$("$BIN" --install spektre-labs/skill-algebra)"
echo "$OK" | grep -q '"status":"ok"' || { echo "v168: healthy install failed"; exit 4; }

echo "[v168] (5) σ-gated refusal"
REF="$("$BIN" --install random/kernel-experimental || true)"
echo "$REF" | grep -q '"status":"refused"' || { echo "v168: missing refuse"; exit 5; }
echo "$REF" | grep -q "use --force"        || { echo "v168: missing force hint"; exit 5; }

echo "[v168] (6) forced install"
FRC="$("$BIN" --install random/kernel-experimental --force)"
echo "$FRC" | grep -q '"status":"forced"' || { echo "v168: force didn't stick"; exit 6; }
echo "$FRC" | grep -q '"forced":true'     || { echo "v168: forced flag missing"; exit 6; }

echo "[v168] (7) search"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v168_marketplace", "--search", "skill"]).decode())
assert doc["query"] == "skill"
assert len(doc["matches"]) >= 2, doc
for m in doc["matches"]:
    assert "skill" in m["id"] or "skill" in m["name"], m
print("search ok", [m["id"] for m in doc["matches"]])
PY

echo "[v168] (8) .cos-skill validator"
OK_FIX="$("$BIN" --validate-cos-skill "adapter.safetensors,template.txt,test.jsonl,meta.toml,README.md")"
echo "$OK_FIX" | grep -q '"status":"ok"'    || { echo "v168: fixture ok path"; exit 8; }
BAD_FIX="$("$BIN" --validate-cos-skill "adapter.safetensors,template.txt,test.jsonl,meta.toml" || true)"
echo "$BAD_FIX" | grep -q '"status":"invalid"' || { echo "v168: fixture bad path"; exit 8; }

echo "[v168] (9) determinism"
A="$("$BIN")"
B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v168: listing not deterministic"; exit 9; }

echo "[v168] ALL CHECKS PASSED"

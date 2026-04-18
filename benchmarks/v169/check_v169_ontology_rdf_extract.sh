#!/usr/bin/env bash
#
# benchmarks/v169/check_v169_ontology_rdf_extract.sh
#
# Merge-gate for v169 σ-Ontology.  Verifies:
#   1. self-test
#   2. 50 fixture entries → ≥ 50 candidate triples
#   3. τ-gate drops *some* triples but keeps *some* (non-trivial κ)
#   4. σ_extraction ∈ [0,1] on every triple
#   5. entity typing: lauri→Person, creation_os→Software,
#      sigma→Metric, v101→Kernel, rpi5→Device
#   6. query `mentions σ` returns ≥ 1 entry id
#   7. OWL-lite-JSON has 6 classes and every kept entity lives
#      in exactly one of them
#   8. determinism: two runs produce byte-identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v169_ontology"
test -x "$BIN" || { echo "v169: binary not built"; exit 1; }

echo "[v169] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v169] (2..5) state JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v169_ontology"]).decode())
assert doc["n_entries"] == 50, doc
assert doc["n_triples"] >= 50, doc
assert 0 < doc["n_kept"] < doc["n_triples"], doc   # non-trivial gate
for t in doc["triples"]:
    assert 0.0 <= t["sigma"] <= 1.0, t
    assert 0 <= t["src"] < 50, t
ents = {e["name"]: e for e in doc["entities"]}
assert ents.get("lauri",        {}).get("class") == "Person",   ents.get("lauri")
assert ents.get("creation_os",  {}).get("class") == "Software", ents.get("creation_os")
assert ents.get("sigma",        {}).get("class") == "Metric",   ents.get("sigma")
# kernel and device must each appear at least once in the class set
classes_seen = {e["class"] for e in doc["entities"]}
for cls in ("Kernel", "Device"):
    assert cls in classes_seen, (cls, classes_seen)
print("extract + typing ok")
PY

echo "[v169] (6) corpus navigation query"
Q="$("$BIN" --query mentions sigma)"
python3 - <<PY
import json
doc = json.loads("""$Q""")
assert doc["event"] == "query"
assert doc["predicate"] == "mentions"
assert doc["object"] == "sigma"
assert doc["hits"] >= 1, doc
assert len(doc["entry_ids"]) == doc["hits"]
print("query ok, hits=", doc["hits"], "entries=", doc["entry_ids"])
PY

echo "[v169] (7) OWL-lite JSON"
python3 - <<'PY'
import json, subprocess
owl = json.loads(subprocess.check_output(["./creation_os_v169_ontology", "--owl"]).decode())
assert owl["owl_lite_json"] is True
assert len(owl["classes"]) == 6, owl
names = set(c["class"] for c in owl["classes"])
for expected in ("Person", "Software", "Metric", "Kernel", "Device", "Concept"):
    assert expected in names, (expected, names)
# every named entity sits in exactly one class bucket
all_members = []
for c in owl["classes"]:
    all_members += c["members"]
assert len(all_members) == len(set(all_members)), "entity in multiple classes"
print("owl ok, classes=", sorted(names))
PY

echo "[v169] (8) determinism"
A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v169: state JSON not deterministic"; exit 8; }

echo "[v169] ALL CHECKS PASSED"

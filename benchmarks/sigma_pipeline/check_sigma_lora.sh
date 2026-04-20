#!/usr/bin/env bash
#
# HERMES-1 smoke test: σ-LoRA on-device fine-tuning.
#
# Pinned facts (per lora_main.c):
#   self_test == 0
#   two adapters trained for 300 SGD steps (factual rank 8, code rank 16)
#   σ_improvement = σ_before - σ_after > 0 for both (SGD drove MSE down)
#   bytes = (rank*in_dim + out_dim*rank) * 4
#   routing: "factual" → factual_v1, "code" → code_v1, "general" → (none)
#   Two invocations produce byte-identical output (deterministic LCG).
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_lora"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_lora: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["module"] == "sigma_lora", doc
assert doc["self_test"] == 0, doc

ads = doc["adapters"]
assert len(ads) == 2, ads
names = [a["name"] for a in ads]
assert names == ["factual_v1", "code_v1"], names

factual, code = ads
assert factual["rank"] == 8 and factual["in_dim"] == 16 and factual["out_dim"] == 8, factual
assert code["rank"] == 16 and code["in_dim"] == 16 and code["out_dim"] == 8, code

# Byte counts must follow the shape formula.
assert factual["bytes"] == (8*16 + 8*8)*4, factual
assert code["bytes"]    == (16*16 + 8*16)*4, code

# σ_improvement must be strictly positive after 300 steps.
for a in ads:
    assert a["sigma_improvement"] > 0.0, a
    assert 0.0 <= a["sigma_after"] <= 1.0, a
    assert 0.0 <= a["sigma_before"] <= 1.0, a
    assert a["steps"] == 300, a

route = doc["routing"]
assert route["factual"] == "factual_v1", route
assert route["code"]    == "code_v1", route
# "general" has no binding → routed name should be empty.
assert route["general"] == "", route

print("check_sigma_lora: PASS", {
    "adapters": len(ads),
    "factual_improvement": factual["sigma_improvement"],
    "code_improvement":    code["sigma_improvement"],
})
PY

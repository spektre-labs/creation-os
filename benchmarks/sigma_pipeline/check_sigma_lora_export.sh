#!/usr/bin/env bash
#
# HERMES-4 smoke test: σ-export signed LoRA adapters.
#
# Pinned facts (per lora_export_main.c):
#   self_test == 0
#   write_rc  == 0
#   peek_rc   == 0
#   read_rc   == 0
#   weights_equal == true
#   tamper_rc == COS_LORA_EXPORT_ERR_MAC (-6)
#   info: name=factual_v1, in_dim=16, out_dim=8, rank=8, alpha=16.0000,
#         benchmark_sigma=0.14, created_unix=1700000000,
#         bytes_weights=768, mac_prefix_hex stable across runs.
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_lora_export"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_lora_export: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys, re
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_lora_export", doc
assert doc["self_test"] == 0, doc
assert doc["write_rc"]  == 0, doc
assert doc["peek_rc"]   == 0, doc
assert doc["read_rc"]   == 0, doc
assert doc["weights_equal"] is True, doc
assert doc["tamper_rc"] == -6, doc  # COS_LORA_EXPORT_ERR_MAC

info = doc["info"]
assert info["name"] == "factual_v1", info
assert info["description"] == "Curated factual adapter", info
assert info["trained_by"]  == "spektre-labs", info
assert info["in_dim"] == 16, info
assert info["out_dim"] == 8, info
assert info["rank"] == 8, info
assert abs(info["alpha"] - 16.0) < 1e-6, info
assert abs(info["benchmark_sigma"] - 0.14) < 1e-4, info
assert info["created_unix"] == 1700000000, info
assert info["bytes_weights"] == (8*16 + 8*8) * 4, info
assert re.fullmatch(r"[0-9a-f]{16}", info["mac_prefix_hex"]), info

print("check_sigma_lora_export: PASS", {
    "bytes_weights": info["bytes_weights"],
    "mac_prefix": info["mac_prefix_hex"],
    "tamper_detected": doc["tamper_rc"] == -6,
})
PY

#!/usr/bin/env bash
#
# FINAL-5 smoke test: σ-Voice — local speech interface driver.
#
# Pinned facts (per voice_main.c three-turn script):
#   self_test         == 0
#   tau_stt           ≈ 0.50
#   tau_response      ≈ 0.70
#   sample_rate       == 16000
#   backend           == "sim"
#   turns[0]  clean_query:    admitted=true,  synthesised=true,
#                              repeat=false, rc=0
#   turns[1]  noisy_capture:  admitted=false, repeat=true,
#                              synthesised=true, rc=-4 (REPEAT),
#                              sigma_stt ≥ τ_stt
#   turns[2]  low_conf_resp:  admitted=true,  synthesised=false,
#                              repeat=false, sigma_response > τ_response
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_voice"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_voice: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_voice", doc
assert doc["self_test"] == 0, doc
assert abs(doc["tau_stt"] - 0.50) < 1e-3, doc
assert abs(doc["tau_response"] - 0.70) < 1e-3, doc
assert doc["sample_rate"] == 16000, doc
assert doc["backend"] == "sim", doc

turns = doc["turns"]
assert len(turns) == 3, turns
labels = [t["label"] for t in turns]
assert labels == ["clean_query", "noisy_capture", "low_conf_resp"], labels

# Turn 0 — clean path
t0 = turns[0]
assert t0["rc"] == 0 and t0["admitted"] and t0["synthesised"] and not t0["repeat"], t0
assert t0["sigma_stt"] <= doc["tau_stt"], t0
assert t0["sigma_response"] <= doc["tau_response"], t0
assert t0["wav_out_bytes"] > 0, t0

# Turn 1 — noisy capture: σ_stt above τ_stt, driver asks to repeat
t1 = turns[1]
assert t1["rc"] == -4, t1                 # COS_VOICE_REPEAT
assert t1["admitted"] is False, t1
assert t1["repeat"] is True, t1
assert t1["sigma_stt"] > doc["tau_stt"], t1
# TTS still synthesises the repeat prompt so the user hears it.
assert t1["synthesised"] is True, t1
assert t1["wav_out_bytes"] > 0, t1
assert "toistaa" in t1["reply"], t1

# Turn 2 — pipeline response σ above τ_response → no synthesis
t2 = turns[2]
assert t2["rc"] == 0 and t2["admitted"] and not t2["repeat"], t2
assert t2["sigma_response"] > doc["tau_response"], t2
assert t2["synthesised"] is False, t2
assert t2["wav_out_bytes"] == 0, t2

# native_available is a deployment fact, not a gate; we simply
# report it so the check is host-agnostic.
assert isinstance(doc["native_available"], bool), doc

print("check_sigma_voice: PASS", {
    "turns": len(turns),
    "native_available": doc["native_available"],
})
PY

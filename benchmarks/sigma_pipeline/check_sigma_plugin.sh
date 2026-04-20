#!/usr/bin/env bash
#
# NEXT-5 smoke test: σ-Plugin — dlopen ABI + registry + tool
# round-trip.
#
# Pinned facts:
#   self_test_rc  == 0                 (registry bounds, duplicate
#                                       rejection, ABI mismatch,
#                                       full-registry all fire)
#   abi_version   == 1                 (COS_SIGMA_PLUGIN_ABI)
#   dlopen.rc     == 0                 (libcos_plugin_demo loaded)
#   tool_rc       == 0
#   tool_stdout   == "prometheus\n"    (demo_echo round-trip)
#   custom_sigma  == 0.1370            (sealed constant in plugin)
#   list.count    == 1
#   list.plugins[0].name    == "demo"
#   list.plugins[0].version == "0.1.0"
#   list.plugins[0].tools   == 2       (demo_echo, demo_rev)
#   list.plugins[0].origin  == "dlopen"
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_plugin"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# The demo shared library must have been built by the Makefile.
shopt -s nullglob
libs=( ./libcos_plugin_demo.dylib ./libcos_plugin_demo.so )
[[ ${#libs[@]} -gt 0 ]] \
    || { echo "missing demo shared library (libcos_plugin_demo.*)" >&2; exit 2; }

OUT="$($BIN)"

for fact in \
    '"self_test_rc":0' \
    '"abi_version":1' \
    '"dlopen":{"tried":' \
    '"rc":0' \
    '"tool_rc":0' \
    '"tool_stdout":"prometheus\n"' \
    '"custom_sigma":0.1370' \
    '"pass":true' \
    '"count":1' \
    '"name":"demo"' \
    '"version":"0.1.0"' \
    '"tools":2' \
    '"origin":"dlopen"'
do
    grep -q -F "$fact" <<<"$OUT" \
        || { echo "FAIL: missing '$fact' in plugin output" >&2
             echo "$OUT"; exit 3; }
done

python3 - "$OUT" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
assert j["abi_version"]  == 1, j
assert j["dlopen"]["rc"] == 0, j
assert j["tool_rc"]      == 0, j
assert j["tool_stdout"]  == "prometheus\n", j
assert abs(j["custom_sigma"] - 0.137) < 1e-4, j
assert j["list"]["count"] == 1
p = j["list"]["plugins"][0]
assert p["name"]    == "demo"
assert p["version"] == "0.1.0"
assert p["tools"]   == 2
assert p["origin"]  == "dlopen"
assert p["has_custom_sigma"] is True
assert p["has_codex"]        is True
print("check-sigma-plugin: PASS (ABI={} dlopen ok, demo_echo round-trip, "
      "custom σ = {:.4f}, registry listing intact)".format(
          j["abi_version"], j["custom_sigma"]))
PY

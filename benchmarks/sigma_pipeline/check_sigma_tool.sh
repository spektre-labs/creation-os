#!/usr/bin/env bash
# σ-Tool smoke test (A1): registry + selector + gate + risk classes.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_tool
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Registry pins:
grep -q '"cap":8,"count":5'                        <<<"$OUT" || { echo "registry"   >&2; exit 5; }
grep -q '"name":"calculator","risk":"SAFE"'        <<<"$OUT" || { echo "calc risk"  >&2; exit 6; }
grep -q '"name":"file_read","risk":"READ"'         <<<"$OUT" || { echo "fread risk" >&2; exit 7; }
grep -q '"name":"file_write","risk":"WRITE"'       <<<"$OUT" || { echo "fwrite risk">&2; exit 8; }
grep -q '"name":"web_fetch","risk":"NETWORK"'      <<<"$OUT" || { echo "fetch risk" >&2; exit 9; }
grep -q '"name":"shell_rm","risk":"IRREVERSIBLE"'  <<<"$OUT" || { echo "rm risk"    >&2; exit 10; }

# Selector pins:
grep -q '"calc":{"tool":"calculator","sigma_sel":0.0500,"gate":"ALLOW"'       <<<"$OUT" \
    || { echo "calc select/gate" >&2; exit 11; }
grep -q '"fread":{"tool":"file_read","sigma_sel":0.0500,"gate":"ALLOW"'       <<<"$OUT" \
    || { echo "fread select/gate" >&2; exit 12; }
grep -q '"rm":{"tool":"shell_rm","sigma_sel":0.0500,"gate":"BLOCK"'           <<<"$OUT" \
    || { echo "rm select/gate (expected BLOCK at base 0.80)" >&2; exit 13; }
grep -q '"none":{"tool":null,"sigma_sel":1.0000}'                             <<<"$OUT" \
    || { echo "no-match" >&2; exit 14; }

# Executor pins:
grep -q '"exec_calc":{"rc":0,"text":"4","sigma_res":0.0200,"decision":"ALLOW"' <<<"$OUT" \
    || { echo "exec result" >&2; exit 15; }
grep -q '"cost_eur":0.000010'                                                  <<<"$OUT" \
    || { echo "cost"        >&2; exit 16; }

echo "check-sigma-tool: PASS (5 tools, 4 risk-class gates, executor stamp)"

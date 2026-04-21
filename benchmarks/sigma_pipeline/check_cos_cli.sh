#!/usr/bin/env bash
# Smoke test for the reference CLIs (I3 + I4).
#
#   cos-chat --banner-only       — banner prints codex + mode + τ
#   cos-benchmark                — 4-row comparison table with
#                                  codex effect line + saved-vs-api
#   cos-cost --json              — canonical ledger matches the
#                                  sigma_sovereign self-test demo
#   cos-cost --from-benchmark    — non-zero cloud calls, saved > 0
set -euo pipefail
cd "$(dirname "$0")/../.."

for bin in cos-chat cos-benchmark cos-cost; do
    [[ -x "./$bin" ]] || { echo "missing ./$bin" >&2; exit 2; }
done

echo "  · cos-chat --banner-only"
B="$(./cos-chat --banner-only)"
grep -q "Creation OS · σ-gated chat" <<<"$B" || { echo "no banner"    >&2; exit 3; }
grep -q "1 = 1"                      <<<"$B" || { echo "no invariant" >&2; exit 4; }
grep -q "codex:  loaded"             <<<"$B" || { echo "codex off"    >&2; exit 5; }

echo "  · cos-chat --no-codex --banner-only"
Bn="$(./cos-chat --no-codex --banner-only)"
grep -q "codex:  off" <<<"$Bn" || { echo "no-codex banner wrong" >&2; exit 6; }

echo "  · cos-benchmark"
BM="$(./cos-benchmark)"
grep -q "bitnet_only"       <<<"$BM" || { echo "no bitnet_only row"  >&2; exit 7; }
grep -q "pipeline_no_codex" <<<"$BM" || { echo "no no-codex row"     >&2; exit 8; }
grep -q "pipeline_codex"    <<<"$BM" || { echo "no codex row"        >&2; exit 9; }
grep -q "api_only"          <<<"$BM" || { echo "no api_only row"     >&2; exit 10; }
grep -qE "saved [0-9]+\.[0-9]%"  <<<"$BM" || { echo "no saved line"  >&2; exit 11; }
grep -q "codex effect"      <<<"$BM" || { echo "no codex effect"     >&2; exit 12; }
grep -q "1 = 1"             <<<"$BM" || { echo "no invariant"        >&2; exit 13; }

echo "  · cos-benchmark --json"
BJ="$(./cos-benchmark --json)"
grep -q '"codex_loaded":true' <<<"$BJ" || { echo "codex not loaded"  >&2; exit 14; }
grep -q '"configs":\['        <<<"$BJ" || { echo "no configs"        >&2; exit 15; }
grep -q '"name":"api_only"'   <<<"$BJ" || { echo "no api_only name"  >&2; exit 16; }

echo "  · cos-benchmark --energy (ULTRA-7)"
BE="$(./cos-benchmark --energy)"
grep -q "reasoning/J" <<<"$BE" || { echo "no reasoning/J header" >&2; exit 27; }
grep -q "bitnet_only" <<<"$BE" || { echo "no bitnet_only energy row" >&2; exit 28; }

echo "  · cos-cost --json (canonical 85/15)"
CJ="$(./cos-cost --json)"
grep -q '"n_local":85'              <<<"$CJ" || { echo "n_local"     >&2; exit 17; }
grep -q '"n_cloud":15'              <<<"$CJ" || { echo "n_cloud"     >&2; exit 18; }
grep -q '"sovereign_fraction":0.8500' <<<"$CJ" || { echo "frac"      >&2; exit 19; }
grep -q '"verdict":"HYBRID"'        <<<"$CJ" || { echo "verdict"     >&2; exit 20; }
grep -qE '"saved_pct":84\.[0-9]{2}' <<<"$CJ" || { echo "saved"       >&2; exit 21; }

echo "  · cos-cost --from-benchmark --json"
CB="$(./cos-cost --from-benchmark --json)"
grep -q '"source":"benchmark"'      <<<"$CB" || { echo "src"         >&2; exit 22; }
grep -qE '"n_cloud":[1-9][0-9]*'    <<<"$CB" || { echo "cloud=0"     >&2; exit 23; }
grep -qE '"saved_pct":(8[0-9]|9[0-9])\.' <<<"$CB" || { echo "saved<80%" >&2; exit 24; }

echo "  · cos-cost interactive text report"
CT="$(./cos-cost)"
grep -q "Creation OS — zero-cloud sovereignty ledger" <<<"$CT" \
    || { echo "no ledger header" >&2; exit 25; }
grep -q "1 = 1"                                      <<<"$CT" \
    || { echo "no invariant"     >&2; exit 26; }

echo "check-cos-cli: PASS (chat banner + benchmark table + cost ledger)"

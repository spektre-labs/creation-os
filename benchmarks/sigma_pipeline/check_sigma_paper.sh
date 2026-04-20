#!/usr/bin/env bash
#
# Smoke test for σ-Paper (H5): deterministic arXiv-style paper
# generator.  The paper pulls every number from the pinned
# spec; this test ensures the fingerprint, ledger numbers, and
# section count cannot drift silently from the code.

set -euo pipefail

BIN=${1:-./creation_os_sigma_paper}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-paper: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                        <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                             <<<"$OUT" || { echo "FAIL pass"; exit 1; }
grep -q '"sections":9'                            <<<"$OUT" || { echo "FAIL sections"; exit 1; }
grep -q '"bytes":5193'                            <<<"$OUT" || { echo "FAIL byte count (spec drift?)"; exit 1; }
grep -q '"fingerprint":"9905916cbff54a86"'        <<<"$OUT" || { echo "FAIL fingerprint"; exit 1; }
grep -q '"t3_witnesses":16384'                    <<<"$OUT" || { echo "FAIL T3 pin"; exit 1; }
grep -q '"t5_witnesses":896'                      <<<"$OUT" || { echo "FAIL T5 pin"; exit 1; }
grep -q '"t6_bound_ns":250'                       <<<"$OUT" || { echo "FAIL T6 bound"; exit 1; }
grep -q '"all_discharged":true'                   <<<"$OUT" || { echo "FAIL ledger"; exit 1; }
grep -q '"substrates_equivalent":true'            <<<"$OUT" || { echo "FAIL substrates"; exit 1; }
grep -q '"pipeline_savings_percent":78.8'         <<<"$OUT" || { echo "FAIL savings"; exit 1; }
grep -q '"hybrid_monthly_eur":4.27'               <<<"$OUT" || { echo "FAIL hybrid EUR"; exit 1; }

# Write mode: generate into a tmp path and verify the bytes
# match the stated count (deterministic output).
TMP=$(mktemp -t sigma_paper.XXXXXX.md)
trap 'rm -f "$TMP"' EXIT
"$BIN" --out "$TMP" >/dev/null
BYTES=$(wc -c < "$TMP" | tr -d ' ')
if [ "$BYTES" != "5193" ]; then
  echo "FAIL byte count on disk: $BYTES (expected 5193)"; exit 1
fi

# Section-heading order check: all nine headings must appear in
# ascending order inside the rendered Markdown.
awk 'BEGIN{expect=1}
     /^## [0-9]+\. /{
       n = substr($2, 1, index($2,".")-1) + 0;
       if (n != expect) { print "heading out of order at #" n " (expected " expect ")"; exit 1 }
       expect++
     }
     END{ if (expect != 10) { print "missing headings: got " (expect-1) "/9"; exit 1 } }' "$TMP" \
  || { echo "FAIL section order"; exit 1; }

echo "check-sigma-paper: 9 sections, 5193 bytes, fingerprint 9905916cbff54a86, pinned ledger intact"

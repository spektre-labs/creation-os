#!/usr/bin/env bash
#
# Smoke test for σ-Protocol (D5): signed binary wire format,
# round-trip encode/decode, and MAC-based tamper / wrong-key
# rejection.

set -euo pipefail

BIN=${1:-./creation_os_sigma_protocol}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-protocol: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                                    <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                         <<<"$OUT" || { echo "FAIL pass flag"; exit 1; }
grep -q '"encode_rc":0'                                       <<<"$OUT" || { echo "FAIL encode_rc"; exit 1; }

# 56-byte header + 32-byte payload + 64-byte sig = 152.
grep -q '"frame_bytes":152'                                   <<<"$OUT" || { echo "FAIL frame size"; exit 1; }
grep -q '"header_bytes":56'                                   <<<"$OUT" || { echo "FAIL header size"; exit 1; }
grep -q '"sig_bytes":64'                                      <<<"$OUT" || { echo "FAIL sig size"; exit 1; }

# Peek identifies the type without verifying the signature.
grep -q '"peek_rc":0'                                         <<<"$OUT" || { echo "FAIL peek_rc"; exit 1; }
grep -q '"peek_type":"QUERY"'                                 <<<"$OUT" || { echo "FAIL peek_type"; exit 1; }

# Full decode round-trip preserves every field.
grep -q '"decode_rc":0'                                       <<<"$OUT" || { echo "FAIL decode_rc"; exit 1; }
grep -q '"sender_id":"node-finland"'                          <<<"$OUT" || { echo "FAIL sender_id"; exit 1; }
grep -q '"sender_sigma":0.2200'                               <<<"$OUT" || { echo "FAIL sigma"; exit 1; }
grep -q '"timestamp_ns":1713500000000000000'                  <<<"$OUT" || { echo "FAIL timestamp"; exit 1; }
grep -q '"payload_len":32'                                    <<<"$OUT" || { echo "FAIL payload_len"; exit 1; }
grep -q '"payload_hex16":"what is ..."'                       <<<"$OUT" || { echo "FAIL payload"; exit 1; }

# FNV-based MAC is deterministic: first 8 bytes must match pin.
grep -q '"sig_prefix8":"b2d1002a72d22435"'                    <<<"$OUT" || { echo "FAIL sig prefix"; exit 1; }

# Tamper + wrong key both rejected with rc=-9.
grep -q '"tamper_rc":-9'                                      <<<"$OUT" || { echo "FAIL tamper reject"; exit 1; }
grep -q '"wrong_key_rc":-9'                                   <<<"$OUT" || { echo "FAIL wrong-key reject"; exit 1; }

echo "check-sigma-protocol: encode + decode + tamper guard + 7-type round-trip green"

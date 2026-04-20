#!/usr/bin/env bash
#
# Smoke test for σ-Protocol Ed25519 signer (FIX-2): real asymmetric
# crypto via vendored orlp/ed25519 replaces the FNV-1a stub for the
# WAN-trust path.  Pins the deterministic keypair derived from a
# fixed seed and the signature over a canonical message.

set -euo pipefail

BIN=${1:-./creation_os_sigma_ed25519}
if [ ! -x "$BIN" ]; then
  echo "check-sigma-ed25519: missing binary $BIN" >&2
  exit 2
fi

OUT=$("$BIN")
echo "$OUT"

grep -q '"self_test_rc":0'                               <<<"$OUT" || { echo "FAIL self_test_rc"; exit 1; }
grep -q '"pass":true'                                    <<<"$OUT" || { echo "FAIL pass"; exit 1; }

# Deterministic seed = bytes 0x00..0x1f → canonical Ed25519 keypair.
grep -q '"seed":"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"' <<<"$OUT" \
  || { echo "FAIL seed pinning"; exit 1; }
grep -q '"pub":"03a107bff3ce10be1d70dd18e74bc09967e4d6309ba50d5f1ddc8664125531b8"' <<<"$OUT" \
  || { echo "FAIL pub pinning (keypair non-deterministic?)"; exit 1; }
grep -q '"signature":"a662a566c604fa1ed36c8bf698c82add4af217af46d60eab445d03fe029a2a0b2e9663bd3b6ece5d84c406551d7be44d230230d54aabbd628029419035213f05"' <<<"$OUT" \
  || { echo "FAIL signature pinning (sign non-deterministic?)"; exit 1; }

grep -q '"verified":true'                                <<<"$OUT" || { echo "FAIL verified"; exit 1; }
grep -q '"tamper_rejected":true'                         <<<"$OUT" || { echo "FAIL tamper reject"; exit 1; }
grep -q '"wrongkey_rejected":true'                       <<<"$OUT" || { echo "FAIL wrongkey reject"; exit 1; }

echo "check-sigma-ed25519: smoke OK"

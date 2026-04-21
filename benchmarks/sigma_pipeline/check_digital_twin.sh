#!/usr/bin/env bash
# HORIZON-2: digital twin preflight + cos-exec harness.
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-exec ]] || { echo "missing ./cos-exec" >&2; exit 2; }

echo "  · cos-exec --self-test"
./cos-exec --self-test

echo "  · cos-exec --simulate --dry-run (missing source → UNSAFE)"
out=$(./cos-exec --simulate --dry-run -- cp "/nonexistent_cos_twin_$$" /tmp/) || true
grep -q 'does NOT exist' <<<"$out" || { echo "$out" >&2; exit 3; }
grep -q 'UNSAFE → aborted' <<<"$out" || { echo "$out" >&2; exit 4; }

echo "  · cos-exec --simulate (real cp under /tmp)"
src="/tmp/cos_twin_harness_src_$$"
dst="/tmp/cos_twin_harness_dst_$$"
printf 'x' >"$src"
rm -f "$dst"
out2=$(./cos-exec --simulate -- cp "$src" "$dst") || { rm -f "$src" "$dst"; exit 5; }
grep -q 'SAFE → executing' <<<"$out2" || { echo "$out2" >&2; rm -f "$src" "$dst"; exit 6; }
grep -q 'exists' <<<"$out2" || { echo "$out2" >&2; rm -f "$src" "$dst"; exit 7; }
[[ -f "$dst" ]] || { echo "dst not created" >&2; rm -f "$src"; exit 8; }
rm -f "$src" "$dst"

echo "check-digital-twin: PASS (self-test + missing-src + cp execute)"

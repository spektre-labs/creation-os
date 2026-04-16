#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS v61 — run v61 self-test under the Darwin sandbox-exec
# profile in sandbox/darwin.sb.  SKIP honestly on non-Darwin.

set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "sandbox-exec: SKIP (Darwin sandbox-exec not on $(uname -s); use sandbox/openbsd_pledge.c on BSD)"
  exit 0
fi
if ! command -v sandbox-exec >/dev/null 2>&1; then
  echo "sandbox-exec: SKIP (Apple sandbox-exec binary not on PATH)"
  exit 0
fi
if ! [ -x ./creation_os_v61 ]; then
  echo "sandbox-exec: SKIP (creation_os_v61 not yet built — run 'make standalone-v61' first)"
  exit 0
fi

echo "sandbox-exec: running v61 self-test under sandbox/darwin.sb"
set +e
sandbox-exec -f sandbox/darwin.sb \
  -D HOME="$HOME" -D CWD="$PWD" \
  ./creation_os_v61 --self-test >/tmp/v61_sandbox.log 2>&1
rc=$?
set -e
tail -5 /tmp/v61_sandbox.log | sed 's/^/  /'
if [ $rc -eq 0 ]; then
  echo "sandbox-exec: OK (v61 self-test passes inside Apple sandbox profile)"
else
  echo "sandbox-exec: FAIL (rc=$rc; see /tmp/v61_sandbox.log)"
  exit $rc
fi

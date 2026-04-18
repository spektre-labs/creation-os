#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v123 σ-Formal merge-gate check.
#
# Two-tier enforcement:
#
#   Tier 1 — ALWAYS runs (no Java, no tlc required):
#     pure-C structural validator confirms the TLA+ spec exists,
#     declares the module / CONSTANTS / vars / Spec skeleton, and
#     lists all seven σ-governance invariants by name.  Any missing
#     piece is a hard FAIL.
#
#   Tier 2 — RUNS WHEN AVAILABLE:
#     full TLC model check via `tlc` on PATH, or
#       `java -cp $TLA_TOOLS_JAR tlc2.TLC` when TLA_TOOLS_JAR is set,
#     or `java -cp $HOME/.tla_tools/tla2tools.jar tlc2.TLC`.
#     On failure (invariant violation) the merge-gate FAILs.  When no
#     TLC is available we SKIP the exhaustive check but Tier 1 still
#     holds the contract.  See docs/v123/README.md for how to drop in
#     `tla2tools.jar` locally.

set -euo pipefail

BIN=${BIN:-./creation_os_v123_formal}
if [ ! -x "$BIN" ]; then
    echo "check-v123: $BIN not found; run \`make creation_os_v123_formal\`" >&2
    exit 1
fi

echo "check-v123: tier-1 structural validator (no Java required)"
"$BIN" --self-test
"$BIN" --validate specs/v123/sigma_governance.tla specs/v123/sigma_governance.cfg >/dev/null

TLA_SPEC=specs/v123/sigma_governance.tla
TLA_CFG=specs/v123/sigma_governance.cfg

# Resolve a runnable TLC invocation.
TLC_CMD=()
if command -v tlc >/dev/null 2>&1; then
    TLC_CMD=(tlc)
elif [ -n "${TLA_TOOLS_JAR:-}" ] && [ -f "$TLA_TOOLS_JAR" ]; then
    TLC_CMD=(java -XX:+UseParallelGC -Xss16M -cp "$TLA_TOOLS_JAR" tlc2.TLC)
elif [ -f "$HOME/.tla_tools/tla2tools.jar" ]; then
    TLC_CMD=(java -XX:+UseParallelGC -Xss16M \
             -cp "$HOME/.tla_tools/tla2tools.jar" tlc2.TLC)
elif [ -f "third_party/tla2tools.jar" ]; then
    TLC_CMD=(java -XX:+UseParallelGC -Xss16M \
             -cp "third_party/tla2tools.jar" tlc2.TLC)
fi

if [ ${#TLC_CMD[@]} -eq 0 ]; then
    echo "check-v123: tier-2 TLC model check SKIPPED (no tlc/tla2tools.jar)"
    echo "check-v123: hint — download TLA+ tools to \$HOME/.tla_tools/tla2tools.jar"
    echo "check-v123: OK (tier-1 contract held; exhaustive check deferred)"
    exit 0
fi

echo "check-v123: tier-2 TLC model check via: ${TLC_CMD[*]}"
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

# TLC expects to run in the directory that contains the spec.
pushd "$(dirname "$TLA_SPEC")" >/dev/null
set +e
"${TLC_CMD[@]}" -config "$(basename "$TLA_CFG")" "$(basename "$TLA_SPEC")" \
    >"$tmp" 2>&1
rc=$?
set -e
popd >/dev/null

if ! grep -q "No error has been found" "$tmp"; then
    echo "check-v123: TLC did not report 'No error has been found':"
    cat "$tmp" | tail -40
    exit 1
fi
if [ "$rc" -ne 0 ]; then
    echo "check-v123: TLC exited with $rc despite 'No error has been found':"
    cat "$tmp" | tail -40
    exit 1
fi

echo "check-v123: OK (tier-1 contract + TLC exhaustive model check PASS)"

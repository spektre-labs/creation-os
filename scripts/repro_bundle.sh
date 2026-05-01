#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
#
# scripts/repro_bundle.sh — deterministic reproduction bundle.
#
# Collects the host metadata, git state, compiler identity, and the
# summary lines from merge-gate + check-sigma-pipeline into a single
# file so every reported Creation OS number travels with the facts
# needed to re-run it.  Matches the checklist in
# docs/REPRO_BUNDLE_TEMPLATE.md.
#
# Usage:
#
#     scripts/repro_bundle.sh                 # writes repro_bundle.txt
#     scripts/repro_bundle.sh --out FILE      # writes FILE
#     scripts/repro_bundle.sh --json          # machine-readable form
#     scripts/repro_bundle.sh --quick         # skip the full make runs
#
# Environment:
#
#     COS_REPRO_QUICK=1      equivalent to --quick (no make invocations).
#     COS_REPRO_CHECK=...    override the make target exercised (default:
#                            "check-sigma-pipeline"), or "none" to skip.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -u

OUT="repro_bundle.txt"
JSON=0
QUICK="${COS_REPRO_QUICK:-0}"
CHECK_TARGET="${COS_REPRO_CHECK:-check-sigma-pipeline}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --out)   OUT="$2"; shift 2 ;;
        --json)  JSON=1;   shift ;;
        --quick) QUICK=1;  shift ;;
        -h|--help)
            sed -n '3,20p' "$0"; exit 0 ;;
        *)
            echo "repro_bundle: unknown arg '$1'" >&2; exit 2 ;;
    esac
done

# -- helpers ----------------------------------------------------------------
trim_nl() { tr -d '\n' | tr -s ' '; }
shell_escape_json() { sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'; }

collect_cpu() {
    if [[ "$(uname -s)" == "Darwin" ]]; then
        sysctl -n machdep.cpu.brand_string 2>/dev/null || echo "unknown"
    elif [[ -r /proc/cpuinfo ]]; then
        grep -m1 '^model name' /proc/cpuinfo | sed 's/^[^:]*: //' || echo "unknown"
    else
        echo "unknown"
    fi
}

collect_cc() {
    ( cc --version 2>/dev/null || gcc --version 2>/dev/null || clang --version 2>/dev/null ) \
        | head -1
}

run_make_summary() {
    local target="$1"
    if [[ "$target" == "none" || "$QUICK" == "1" ]]; then
        echo "(skipped)"
        return 0
    fi
    local out rc
    out="$(make "$target" 2>&1)"; rc=$?
    if [[ $rc -ne 0 ]]; then
        echo "FAIL rc=$rc — $(printf '%s\n' "$out" | tail -1)"
    else
        printf '%s\n' "$out" | tail -1
    fi
}

# -- gather -----------------------------------------------------------------
DATE_UTC="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
HOSTNAME="$(hostname 2>/dev/null || echo unknown)"
UNAME="$(uname -a 2>/dev/null || echo unknown)"
CPU="$(collect_cpu)"
OS="$(uname -sr 2>/dev/null || echo unknown)"
CC_LINE="$(collect_cc | trim_nl)"
GIT_SHA="$(git rev-parse HEAD 2>/dev/null || echo unknown)"
GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo unknown)"
GIT_DIRTY="$(git status --porcelain 2>/dev/null | wc -l | tr -d ' ')"
MERGE_SUMMARY="$(run_make_summary merge-gate)"
PIPELINE_SUMMARY="$(run_make_summary "$CHECK_TARGET")"

# -- render -----------------------------------------------------------------
if [[ "$JSON" == "1" ]]; then
    {
        printf '{'
        printf '"tool":"cos repro_bundle",'
        printf '"date_utc":"%s",' "$DATE_UTC"
        printf '"hostname":"%s",' "$(printf '%s' "$HOSTNAME" | shell_escape_json)"
        printf '"uname":"%s",'    "$(printf '%s' "$UNAME"    | shell_escape_json)"
        printf '"cpu":"%s",'      "$(printf '%s' "$CPU"      | shell_escape_json)"
        printf '"os":"%s",'       "$(printf '%s' "$OS"       | shell_escape_json)"
        printf '"cc":"%s",'       "$(printf '%s' "$CC_LINE"  | shell_escape_json)"
        printf '"git_sha":"%s",'  "$GIT_SHA"
        printf '"git_branch":"%s",' "$(printf '%s' "$GIT_BRANCH" | shell_escape_json)"
        printf '"git_dirty_files":%s,' "${GIT_DIRTY:-0}"
        printf '"merge_gate":"%s",' "$(printf '%s' "$MERGE_SUMMARY"  | shell_escape_json)"
        printf '"%s":"%s"' "$CHECK_TARGET" "$(printf '%s' "$PIPELINE_SUMMARY" | shell_escape_json)"
        printf '}\n'
    } > "$OUT"
else
    {
        echo "=== CREATION OS · REPRO BUNDLE ==="
        echo "date_utc         : $DATE_UTC"
        echo "hostname         : $HOSTNAME"
        echo "uname            : $UNAME"
        echo "cpu              : $CPU"
        echo "os               : $OS"
        echo "cc               : $CC_LINE"
        echo "git_sha          : $GIT_SHA"
        echo "git_branch       : $GIT_BRANCH"
        echo "git_dirty_files  : $GIT_DIRTY"
        echo "merge_gate       : $MERGE_SUMMARY"
        echo "$CHECK_TARGET  : $PIPELINE_SUMMARY"
        echo "=== END ==="
    } > "$OUT"
fi

echo "repro_bundle: wrote $OUT"

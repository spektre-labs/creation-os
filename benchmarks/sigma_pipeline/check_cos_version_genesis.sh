#!/usr/bin/env bash
#
# PROD-6 smoke test: canonical release identity coherence.
#
# Guarantees that the three sources of truth for "Creation OS
# v1.0.0 Genesis" stay byte-compatible:
#
#   1. include/cos_version.h         — compile-time constants
#   2. CHANGELOG.md                  — human-readable changelog
#   3. `cos --version` first 4 lines — runtime banner
#
# Additionally checks (when run on a tagged worktree) that the
# Git tag name matches `v<COS_VERSION_STRING>`; skipped on an
# untagged worktree so the smoke test still passes during
# development.
#
set -euo pipefail
# `cos --version` prints a long banner + many per-kernel lines; the
# smoke test only cares about the first handful of lines.  We
# capture the entire output up-front so SIGPIPE from `head` cannot
# blow up the script under `pipefail`.
cd "$(dirname "$0")/../.."

H="include/cos_version.h"
CL="CHANGELOG.md"
COS="./cos"

[[ -f "$H"   ]] || { echo "missing $H"  >&2; exit 2; }
[[ -f "$CL"  ]] || { echo "missing $CL" >&2; exit 2; }
[[ -x "$COS" ]] || { echo "missing $COS (run 'make cos' first)" >&2; exit 2; }

# ---- 1. pull version from header ------------------------------------------
VS="$(awk -F\" '/COS_VERSION_STRING/ && NF >= 2 { print $2; exit }' "$H")"
CN="$(awk -F\" '/COS_CODENAME/       && NF >= 2 { print $2; exit }' "$H")"
RD="$(awk -F\" '/COS_RELEASE_DATE/   && NF >= 2 { print $2; exit }' "$H")"
[[ -n "$VS" ]] || { echo "FAIL: could not parse COS_VERSION_STRING" >&2; exit 3; }
[[ -n "$CN" ]] || { echo "FAIL: could not parse COS_CODENAME"       >&2; exit 3; }

# ---- 2. header must be semver ---------------------------------------------
[[ "$VS" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] \
    || { echo "FAIL: $VS is not SemVer" >&2; exit 4; }

# ---- 3. CHANGELOG top-most entry must match header ------------------------
top_line="$(grep -E '^## v[0-9]+\.[0-9]+\.[0-9]+' "$CL" | head -n 1)"
grep -Fq "v${VS}" <<<"$top_line" \
    || { echo "FAIL: CHANGELOG top entry '$top_line' does not match $VS" >&2; exit 5; }
grep -Fq "\"${CN}\"" <<<"$top_line" \
    || { echo "FAIL: CHANGELOG top entry '$top_line' missing codename '$CN'" >&2; exit 5; }

# ---- 4. cos --version first line must carry banner ------------------------
VERSION_OUT="$($COS --version)"
first="$(head -n 1 <<<"$VERSION_OUT")"
grep -Fq "Creation OS v${VS}" <<<"$first" \
    || { echo "FAIL: cos --version first line = '$first' (expected 'Creation OS v$VS ...')" >&2; exit 6; }
grep -Fq "\"${CN}\"" <<<"$first" \
    || { echo "FAIL: cos --version first line missing codename" >&2; exit 6; }
grep -Fq "($RD)" <<<"$first" \
    || { echo "FAIL: cos --version first line missing release date" >&2; exit 6; }

# ---- 5. substrates / primitives in line 2 ---------------------------------
line2="$(sed -n 2p <<<"$VERSION_OUT")"
grep -q 'σ-primitives'  <<<"$line2" || { echo "FAIL: line 2 missing primitives" >&2; exit 7; }
grep -q 'check targets' <<<"$line2" || { echo "FAIL: line 2 missing check targets" >&2; exit 7; }
grep -q 'substrates'    <<<"$line2" || { echo "FAIL: line 2 missing substrates"    >&2; exit 7; }

# ---- 6. assert(declared == realized) tagline  ----------------------------
head -n 4 <<<"$VERSION_OUT" | grep -Fq "assert(declared == realized)" \
    || { echo "FAIL: tagline missing from cos --version" >&2; exit 8; }

# ---- 7. optional: Git tag match -------------------------------------------
if tag="$(git describe --exact-match --tags 2>/dev/null)"; then
    case "$tag" in
        v${VS}|v${VS}-*) echo "  git tag: $tag (matches)" ;;
        *) echo "FAIL: git tag '$tag' does not match v$VS" >&2; exit 9 ;;
    esac
else
    echo "  (no exact-match Git tag — development worktree, skipping tag check)"
fi

echo "check-cos-version-genesis: PASS (header v$VS '$CN' == changelog == cos --version)"

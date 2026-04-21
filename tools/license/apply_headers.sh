#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# tools/license/apply_headers.sh
#
# Idempotently inject the canonical SCSL-1.0 / AGPL-3.0 dual-license
# SPDX header at the top of every source file under tracked source
# directories. Files that already carry an SPDX-License-Identifier
# line are left alone (this script is non-destructive).
#
# Usage:
#   tools/license/apply_headers.sh             # apply (modifies files)
#   tools/license/apply_headers.sh --dry-run   # preview only
#   tools/license/apply_headers.sh --check     # exit 1 if any file
#                                              # under scope is missing
#                                              # the SPDX header
#
# This script is the reference implementation for SCSL-1.0 §6
# (Attribution and SPDX Headers).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${REPO_ROOT}"

MODE="${1:-apply}"
case "${MODE}" in
    apply|--apply)   MODE=apply ;;
    --dry-run|-n)    MODE=dry  ;;
    --check|-c)      MODE=check ;;
    --help|-h)
        sed -n '2,20p' "$0"
        exit 0
        ;;
    *)
        echo "apply_headers: unknown mode: ${MODE}" >&2
        exit 2
        ;;
esac

# Source directories in scope. Add new dirs here as the tree grows.
SCOPE_DIRS=(
    "src"
    "cli"
    "core"
    "tools"
    "scripts"
    "tests"
)

# Files in repo root that are *not* third-party and *should* carry headers.
ROOT_SCOPE=(
    "Makefile"
    "Justfile"
    "Earthfile"
)

# File extension -> comment style.
#   c, h, cc, cpp, hpp, m, mm, metal, swift, rs, go, java, kt, ts, js, css, scss
#       => /* ... */
#   sh, bash, zsh, py, rb, pl, mk, Makefile, yml, yaml, toml, dockerfile, conf
#       => # ...
#   md, html, xml, svg
#       => <!-- ... -->
#   sql, lua, hs                 => -- ...
#   verilog, sv, systemverilog   => // ...
header_for() {
    local f="$1"
    local base
    base="$(basename "$f")"
    local ext="${f##*.}"

    case "${base}" in
        Makefile|GNUmakefile|*.mk|Justfile|Earthfile|.envrc|*.toml|*.yml|*.yaml|*.cfg|*.conf|*.ini|*.sh|*.bash|*.zsh|*.py|*.rb|Dockerfile|Dockerfile.*)
            cat <<'EOF'
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
EOF
            return
            ;;
    esac

    case "${ext}" in
        c|h|cc|cpp|cxx|hpp|hxx|m|mm|metal|swift|rs|go|java|kt|kts|ts|tsx|js|jsx|css|scss|less|json5)
            cat <<'EOF'
/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
EOF
            ;;
        sv|v|vh|svh|verilog|systemverilog)
            cat <<'EOF'
// SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
// SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
// Source:        https://github.com/spektre-labs/creation-os-kernel
// Website:       https://spektrelabs.org
// Commercial:    spektre.labs@proton.me
// License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
EOF
            ;;
        sql|lua|hs|elm)
            cat <<'EOF'
-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-- SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
-- Source:        https://github.com/spektre-labs/creation-os-kernel
-- Website:       https://spektrelabs.org
-- Commercial:    spektre.labs@proton.me
-- License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
EOF
            ;;
        md|html|htm|xml|svg)
            cat <<'EOF'
<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
EOF
            ;;
        *)
            return 1
            ;;
    esac
}

has_spdx() {
    head -n 5 "$1" 2>/dev/null | grep -q 'SPDX-License-Identifier'
}

# Skip vendored / third-party trees, build artefacts, dotdirs, and binaries.
SKIP_PATTERN='/\(\.git\|\.build\|\.cache\|\.venv\|node_modules\|dist\|build\|__pycache__\|third_party\|vendor\|\.pytest_cache\|benchmarks/cache\)/'

list_scope_files() {
    {
        for d in "${SCOPE_DIRS[@]}"; do
            [ -d "${d}" ] && find "${d}" -type f
        done
        for f in "${ROOT_SCOPE[@]}"; do
            [ -f "${f}" ] && printf '%s\n' "${f}"
        done
    } | grep -Ev "${SKIP_PATTERN}" | sort -u
}

apply_to_file() {
    local f="$1" mode="$2"
    if has_spdx "${f}"; then
        return 0
    fi
    local hdr
    if ! hdr="$(header_for "${f}")"; then
        # Unknown extension: leave alone.
        return 0
    fi
    case "${mode}" in
        check)
            echo "MISSING-HEADER  ${f}"
            return 7
            ;;
        dry)
            echo "would-add       ${f}"
            ;;
        apply)
            local tmp
            tmp="$(mktemp "${f}.spdx.XXXXXX")"
            # Preserve shebang on first line if present.
            if head -n 1 "${f}" | grep -q '^#!'; then
                head -n 1 "${f}" > "${tmp}"
                printf '%s\n' "${hdr}" >> "${tmp}"
                tail -n +2 "${f}" >> "${tmp}"
            else
                printf '%s\n' "${hdr}" > "${tmp}"
                cat "${f}" >> "${tmp}"
            fi
            mv "${tmp}" "${f}"
            echo "added           ${f}"
            ;;
    esac
}

EXIT=0
COUNT=0
ADDED=0
MISSING=0

while IFS= read -r f; do
    COUNT=$((COUNT+1))
    if apply_to_file "${f}" "${MODE}"; then
        case "${MODE}" in
            apply) has_spdx "${f}" && ADDED=$((ADDED+1)) ;;
        esac
    else
        EXIT=1
        MISSING=$((MISSING+1))
    fi
done < <(list_scope_files)

case "${MODE}" in
    apply)
        echo "apply_headers: scanned ${COUNT} files, headers present after run on $(grep -lR 'SPDX-License-Identifier: LicenseRef-SCSL-1.0' "${SCOPE_DIRS[@]}" 2>/dev/null | wc -l | tr -d ' ') files"
        ;;
    dry)
        echo "apply_headers: dry-run scanned ${COUNT} files"
        ;;
    check)
        if [ "${EXIT}" -ne 0 ]; then
            echo "apply_headers: ${MISSING} file(s) missing the SPDX header" >&2
        else
            echo "apply_headers: OK (${COUNT} files in scope, all carry SPDX header)"
        fi
        ;;
esac

exit "${EXIT}"

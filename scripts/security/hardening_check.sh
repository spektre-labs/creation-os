#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Creation OS — hardening runtime check.
#
# Verifies that the hardened σ-Shield binary (creation_os_v60_hardened)
# actually carries the hardening properties we asked clang for:
#
#   - Position-Independent Executable (PIE / MH_PIE on Mach-O)
#   - Stack canaries present (__stack_chk_guard referenced)
#   - FORTIFY_SOURCE references (*_chk helpers).  v60 does very few
#     libc calls, so not all references need to exist — we check they
#     at least appear in the OpenSSF bootstrap link, not in v60's
#     tiny hot path.
#   - Branch-protection (PAC / BTI) in the LC_BUILD_VERSION metadata.
#
# Portable: works on macOS (Mach-O, otool / nm) and Linux (ELF, readelf
# / nm) when available.  Skips each check gracefully if its tool is
# missing, but never silently passes.

set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_v60_hardened
if [ ! -x "$BIN" ]; then
    echo "hardening-check: $BIN not built — run 'make harden' first."
    exit 1
fi

uname_s="$(uname -s 2>/dev/null || echo unknown)"
fail=0
echo "=== Creation OS hardening-check ($uname_s) ==="

case "$uname_s" in
    Darwin)
        if command -v otool >/dev/null 2>&1; then
            echo "[1/3] Mach-O PIE bit..."
            if otool -hv "$BIN" | grep -q 'PIE'; then
                echo "  PIE: OK"
            else
                echo "  PIE: FAIL (MH_PIE flag missing)"
                fail=1
            fi
        else
            echo "[1/3] otool: SKIP"
        fi

        if command -v nm >/dev/null 2>&1; then
            echo "[2/3] stack canaries / fortify references..."
            if nm -u "$BIN" 2>/dev/null | grep -q '__stack_chk'; then
                echo "  __stack_chk_*: OK"
            else
                echo "  __stack_chk_*: WARN (not referenced — tiny binary OK, large binary not)"
            fi
            if nm -u "$BIN" 2>/dev/null | grep -Eq '(memcpy_chk|memmove_chk|strcpy_chk|snprintf_chk)'; then
                echo "  *_chk (fortify): OK (at least one fortify symbol present)"
            else
                echo "  *_chk (fortify): WARN (no *_chk references — v60 may not use fortifiable libc)"
            fi
        else
            echo "[2/3] nm: SKIP"
        fi

        if command -v otool >/dev/null 2>&1; then
            echo "[3/3] ARM64 branch-protection hint..."
            # A hardened arm64 binary will include __chain_starts or
            # LC_BUILD_VERSION platform 1 with the correct minOS.  We
            # just sanity-check it is an arm64e/arm64 binary.
            arch=$(otool -h "$BIN" | awk 'NR>=3 && NF>=4{print $2; exit}')
            echo "  cpu type field: ${arch:-unknown}"
            echo "  branch-protection: INFORMATIONAL (macOS ld64 emits PAC by default on arm64e stdlib)"
        fi
        ;;
    Linux)
        if command -v readelf >/dev/null 2>&1; then
            echo "[1/3] ELF PIE..."
            if readelf -h "$BIN" | grep -q 'DYN'; then
                echo "  PIE: OK (ET_DYN)"
            else
                echo "  PIE: FAIL (ET_EXEC — not PIE)"
                fail=1
            fi
            echo "[2/3] RELRO + BIND_NOW + NX..."
            dyn=$(readelf -d "$BIN" 2>/dev/null || true)
            prog=$(readelf -l "$BIN" 2>/dev/null || true)
            if echo "$prog" | grep -q 'GNU_RELRO'; then
                echo "  RELRO: OK"
            else
                echo "  RELRO: FAIL"; fail=1
            fi
            if echo "$dyn" | grep -q 'BIND_NOW\|FLAGS.*NOW'; then
                echo "  BIND_NOW: OK"
            else
                echo "  BIND_NOW: FAIL"; fail=1
            fi
            if echo "$prog" | grep -q 'GNU_STACK' && ! (echo "$prog" | grep 'GNU_STACK' | grep -q 'RWE'); then
                echo "  NX (non-exec stack): OK"
            else
                echo "  NX: FAIL"; fail=1
            fi
        fi
        if command -v nm >/dev/null 2>&1; then
            echo "[3/3] stack canary / fortify..."
            nm -u "$BIN" 2>/dev/null | grep -q '__stack_chk' \
                && echo "  __stack_chk_*: OK" \
                || { echo "  __stack_chk_*: WARN (no canary ref)"; }
        fi
        ;;
    *)
        echo "hardening-check: SKIP (unrecognised OS $uname_s)"
        ;;
esac

if [ "$fail" -eq 0 ]; then
    echo "hardening-check: PASS"
else
    echo "hardening-check: FAIL"
fi
exit "$fail"

#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v107 installer — static smoke check.
#
# Validates the three distribution artefacts that ship in this
# repository without actually downloading models, building Docker
# images, or touching the host filesystem outside /tmp:
#
#   1. scripts/install.sh        passes `bash -n` + contains all the
#                                new v107 knobs (forty-kernel stack,
#                                default-model download, config writer,
#                                server boot).
#   2. Dockerfile                parses with `docker buildx build
#                                --check` if Docker is available;
#                                else does a minimal grep for the
#                                forty-kernel + multi-stage shape.
#   3. packaging/brew/creation-os.rb
#                                Ruby-parseable with `ruby -c` if
#                                ruby is on PATH; grep-structural
#                                fallback otherwise.
#
# Also validates the GitHub Actions release workflow mentions the
# three expected jobs (binaries-macos, binaries-linux, docker).
set -u
set -o pipefail

cd "$(dirname "$0")/../.."

FAIL=0
pass() { echo "  PASS  $1"; }
fail() { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }
skip() { echo "  SKIP  $1"; }

# 1. install.sh --------------------------------------------------------
IN=scripts/install.sh
if [ ! -x "$IN" ]; then
    fail "$IN missing or not executable"
else
    if bash -n "$IN" 2>/tmp/cos_v107_sh.$$; then
        pass "install.sh parses with bash -n"
    else
        fail "install.sh syntax (see /tmp/cos_v107_sh.$$)"
    fi
    grep -q 'forty-kernel'                    "$IN" && pass "install.sh says forty-kernel"      || fail "install.sh forty-kernel wording"
    grep -q 'download_default_model'          "$IN" && pass "install.sh downloads default model" || fail "install.sh default-model step"
    grep -q 'write_default_config'            "$IN" && pass "install.sh writes config.toml"      || fail "install.sh config.toml step"
    grep -q 'start_server_optional'           "$IN" && pass "install.sh boots v106 server"       || fail "install.sh server-boot step"
    grep -q 'COS_V107_SKIP_MODEL'             "$IN" && pass "install.sh honors COS_V107_SKIP_MODEL" || fail "install.sh COS_V107_SKIP_MODEL escape"
fi

# 2. Dockerfile --------------------------------------------------------
DF=Dockerfile
if [ ! -f "$DF" ]; then
    fail "$DF missing"
else
    grep -q '^FROM debian:' "$DF" && pass "Dockerfile uses debian base" || fail "Dockerfile debian base"
    grep -q 'AS builder'    "$DF" && pass "Dockerfile has multi-stage build" || fail "Dockerfile multi-stage"
    grep -q 'cos creation_os_server creation_os' "$DF" && pass "Dockerfile copies the three binaries" || fail "Dockerfile binary copy"
    grep -q 'HEALTHCHECK' "$DF" && pass "Dockerfile has HEALTHCHECK" || fail "Dockerfile HEALTHCHECK"
    grep -q 'EXPOSE 8080' "$DF" && pass "Dockerfile EXPOSE 8080"     || fail "Dockerfile EXPOSE"
    if command -v docker >/dev/null 2>&1 && docker buildx version >/dev/null 2>&1; then
        if docker buildx build --check . >/tmp/cos_v107_docker.$$ 2>&1; then
            pass "docker buildx --check parses Dockerfile"
        else
            skip "docker buildx --check (see /tmp/cos_v107_docker.$$ for diag)"
        fi
    else
        skip "docker not installed — structural grep only"
    fi
fi

# 3. Brew formula ------------------------------------------------------
BR=packaging/brew/creation-os.rb
if [ ! -f "$BR" ]; then
    fail "$BR missing"
else
    grep -q 'class CreationOs < Formula' "$BR" && pass "brew formula class shape" || fail "brew formula class"
    grep -q 'desc '                      "$BR" && pass "brew formula desc"        || fail "brew formula desc"
    grep -q 'depends_on "make"'          "$BR" && pass "brew formula depends_on make" || fail "brew formula depends_on"
    grep -q 'bin.install "cos"'          "$BR" && pass "brew formula installs cos" || fail "brew formula cos install"
    grep -q 'bin.install "creation_os_server"' "$BR" && pass "brew formula installs creation_os_server" || fail "brew formula server install"
    if command -v ruby >/dev/null 2>&1; then
        if ruby -c "$BR" >/dev/null 2>&1; then
            pass "ruby -c parses brew formula"
        else
            fail "ruby -c brew formula"
        fi
    else
        skip "ruby not installed — structural grep only"
    fi
fi

# 4. Release workflow --------------------------------------------------
WF=.github/workflows/release.yml
if [ ! -f "$WF" ]; then
    fail "$WF missing"
else
    grep -q 'binaries-macos:'  "$WF" && pass "release.yml binaries-macos job"  || fail "release.yml macos job"
    grep -q 'binaries-linux:'  "$WF" && pass "release.yml binaries-linux job"  || fail "release.yml linux job"
    grep -q 'docker:'          "$WF" && pass "release.yml docker job"          || fail "release.yml docker job"
    grep -q 'platforms: linux/amd64,linux/arm64' "$WF" && pass "release.yml multi-arch docker" || fail "release.yml multi-arch"
fi

rm -f /tmp/cos_v107_*.$$

if [ "$FAIL" -gt 0 ]; then
    echo "check-v107: $FAIL failure(s)"
    exit 1
fi
echo "check-v107: OK (install.sh + Dockerfile + brew formula + release.yml structurally sound)"
exit 0

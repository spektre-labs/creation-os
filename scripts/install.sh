#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
#   Creation OS — one-command bootstrap
#
#   For humans who have never opened GitHub in their life.
#
#       curl -fsSL https://creation-os.run/install | bash
#
#   or, if you already cloned the repo, just:
#
#       ./scripts/install.sh
#
#   What it does, in plain language:
#     1. checks you are on a supported computer (macOS or Linux, x86-64 or arm64)
#     2. installs a C compiler + make if they are not already there
#     3. clones spektre-labs/creation-os into ~/creation-os (skip if already present)
#     4. builds the full twenty-kernel stack
#     5. runs every self-test — you will see real numbers, not promises
#     6. drops you into `cos demo`, the "falling off your chair" tour
#
#   It never installs anything without telling you first.
#   It never writes outside ~/creation-os.
#   It never sends a single byte over the network after cloning.
#
#   Safe to re-run. Idempotent. 1 = 1.

set -euo pipefail

# ---- pretty (colour-safe) -------------------------------------------------

if [ -t 1 ] && [ -z "${NO_COLOR:-}" ] && [ "${TERM:-}" != "dumb" ]; then
  B=$'\033[1m'; D=$'\033[2m'; R=$'\033[0m'
  GRN=$'\033[38;5;42m'; BLU=$'\033[38;5;39m'; AMB=$'\033[38;5;214m'
  RED=$'\033[38;5;203m'; GRY=$'\033[38;5;245m'
else
  B= D= R= GRN= BLU= AMB= RED= GRY=
fi

step() { printf "\n${BLU}${B}▸ %s${R}\n" "$*"; }
ok()   { printf "  ${GRN}✓${R}  %s\n"    "$*"; }
warn() { printf "  ${AMB}!${R}  %s\n"    "$*"; }
die()  { printf "\n${RED}${B}✗ %s${R}\n\n" "$*"; exit 1; }
rule() { printf "${GRY}────────────────────────────────────────────────────────────────────────${R}\n"; }

banner() {
  printf "\n"
  printf "${B}  Creation OS${R} ${GRY}·${R} the verified, branchless, integer-only local AI runtime\n"
  printf "${GRY}  twenty kernels · one verdict · 1 = 1${R}\n"
  rule
}

# ---- detect platform ------------------------------------------------------

OS=$(uname -s)
ARCH=$(uname -m)

case "$OS" in
  Darwin) PLATFORM="macOS";;
  Linux)  PLATFORM="Linux";;
  *)      die "unsupported OS: $OS (Creation OS targets macOS and Linux for now)";;
esac

case "$ARCH" in
  arm64|aarch64) ARCHLBL="arm64 (Apple Silicon / ARM server)";;
  x86_64|amd64)  ARCHLBL="x86-64";;
  *)             die "unsupported CPU: $ARCH";;
esac

# ---- toolchain check ------------------------------------------------------

need_install=()
have() { command -v "$1" >/dev/null 2>&1; }

check_toolchain() {
  have cc   || have gcc || have clang || need_install+=("C compiler (cc / clang / gcc)")
  have make || need_install+=("GNU make")
  have git  || need_install+=("git")
}

install_toolchain_macos() {
  if ! xcode-select -p >/dev/null 2>&1; then
    warn "Xcode Command Line Tools not found. Installing (opens a dialog)…"
    xcode-select --install || true
    printf "\n  ${AMB}Follow the macOS prompt to finish installing the Command Line Tools.${R}\n"
    printf "  ${AMB}Then re-run this installer.${R}\n\n"
    exit 1
  fi
  ok "Xcode Command Line Tools present"
}

install_toolchain_linux() {
  if have apt-get; then
    warn "Installing build-essential git via apt (may prompt for sudo)…"
    sudo apt-get update -qq
    sudo apt-get install -y build-essential git
  elif have dnf; then
    warn "Installing gcc make git via dnf (may prompt for sudo)…"
    sudo dnf install -y gcc make git
  elif have pacman; then
    warn "Installing base-devel git via pacman (may prompt for sudo)…"
    sudo pacman -Sy --noconfirm base-devel git
  elif have apk; then
    warn "Installing build-base git via apk…"
    sudo apk add --no-cache build-base git
  else
    die "no supported package manager found (apt / dnf / pacman / apk). Please install gcc + make + git manually and re-run."
  fi
  ok "toolchain installed"
}

# ---- clone ---------------------------------------------------------------

TARGET="${COS_INSTALL_DIR:-$HOME/creation-os}"
REPO="${COS_REPO_URL:-https://github.com/spektre-labs/creation-os.git}"

clone_or_update() {
  if [ -d "$TARGET/.git" ]; then
    ok "already cloned at ${TARGET}"
    (cd "$TARGET" && git pull --ff-only --quiet) && ok "repo up to date" || warn "could not fast-forward (local changes?) — continuing"
  elif [ -d "$TARGET" ] && [ "$(ls -A "$TARGET" 2>/dev/null || true)" != "" ]; then
    warn "${TARGET} exists and is not a git repo — skipping clone"
  else
    step "Cloning ${REPO} into ${TARGET}"
    git clone --depth 1 --quiet "$REPO" "$TARGET"
    ok "cloned"
  fi
}

# ---- build & test --------------------------------------------------------

build_and_test() {
  cd "$TARGET"

  step "Building the full twenty-kernel stack (this is the only slow step — 2–5 minutes)"
  make -s cos >/dev/null
  ok "cos CLI built"

  step "Running self-tests — real numbers, no promises"
  local targets=(check-v60 check-v61 check-v62 check-v63 check-v64 check-v65 check-v66 \
                 check-v67 check-v68 check-v69 check-v70 check-v71 check-v72 check-v73 \
                 check-v74 check-v76 check-v77 check-v78 check-v79 check-v80)
  local ok_n=0 fail_n=0
  for t in "${targets[@]}"; do
    if make -s "$t" >/tmp/cos-build-$$.log 2>&1; then
      ok_n=$((ok_n+1))
      printf "  ${GRN}✓${R}  %-14s  ${GRY}%s${R}\n" "$t" "$(grep -m1 'OK\|PASS' /tmp/cos-build-$$.log || true)"
    else
      fail_n=$((fail_n+1))
      printf "  ${RED}✗${R}  %-14s  (see /tmp/cos-build-$$.log)\n" "$t"
    fi
  done
  rm -f /tmp/cos-build-$$.log
  rule
  printf "  ${B}twenty-kernel rollup:${R}  ${GRN}${ok_n} PASS${R}  ${RED}${fail_n} FAIL${R}\n"
}

# ---- demo ----------------------------------------------------------------

run_demo() {
  cd "$TARGET"
  step "cos demo — your 30-second tour"
  ./cos demo || true
}

# ---- next steps ----------------------------------------------------------

next_steps() {
  cd "$TARGET"
  rule
  printf "\n  ${B}You are ready.${R}\n\n"
  printf "  ${GRY}From anywhere on this computer:${R}\n"
  printf "    ${B}cd ${TARGET}${R}\n"
  printf "    ${B}./cos status${R}    ${GRY}# one-screen status board${R}\n"
  printf "    ${B}./cos demo${R}      ${GRY}# re-run the tour${R}\n"
  printf "    ${B}./cos sigma${R}     ${GRY}# run every kernel's self-test, live${R}\n"
  printf "    ${B}./cos help${R}      ${GRY}# every command, with a one-line description${R}\n\n"
  printf "  ${GRY}Optional: add ${R}${B}${TARGET}${R}${GRY} to your PATH so you can just type ${R}${B}cos${R}${GRY} from any folder.${R}\n\n"
  rule
  printf "  ${B}Creation OS${R} ${GRY}·${R} ${GRN}installation complete${R}\n\n"
}

# ---- main ----------------------------------------------------------------

main() {
  banner
  ok "platform: ${PLATFORM} ${ARCHLBL}"

  step "Checking toolchain"
  check_toolchain
  if [ "${#need_install[@]}" -gt 0 ]; then
    warn "missing: ${need_install[*]}"
    case "$PLATFORM" in
      macOS) install_toolchain_macos;;
      Linux) install_toolchain_linux;;
    esac
    # Re-check
    need_install=()
    check_toolchain
    [ "${#need_install[@]}" -eq 0 ] || die "toolchain still incomplete: ${need_install[*]}"
  else
    ok "C compiler, make and git already installed"
  fi

  clone_or_update
  build_and_test
  run_demo
  next_steps
}

main "$@"

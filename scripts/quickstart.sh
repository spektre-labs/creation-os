#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# One-command first run: build `cos`, optional cos-demo symlink, then `./cos demo`.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "Creation OS — installing (core build + instant demo)..."

if [[ "${OSTYPE:-}" == darwin* ]]; then
	if ! command -v brew >/dev/null 2>&1; then
		echo "Install Homebrew first: https://brew.sh"
		exit 1
	fi
	brew install cmake llama.cpp 2>/dev/null || true
elif [[ "${OSTYPE:-}" == linux-gnu* ]]; then
	if command -v sudo >/dev/null 2>&1; then
		sudo apt-get update -qq 2>/dev/null || true
		sudo apt-get install -y cmake build-essential 2>/dev/null || true
	fi
fi

make cos cos-demo

echo ""
echo "Build complete. Starting demo..."
echo ""
./cos demo

echo ""
echo "Live inference (~1.2 GB model once): ./scripts/install.sh"
echo ""

#!/bin/sh
# SPDX-License-Identifier: AGPL-3.0-or-later
# Local-first orientation: builds the OpenAI-shaped stub; does not download weights.
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

echo "=== Creation OS — local OpenAI stub (orientation) ==="
echo ""
echo "This tree ships a **stub** HTTP server for protocol wiring:"
echo "  make standalone-openai-stub"
echo "  ./creation_os_openai_stub --port 8080"
echo ""
echo "Docs:"
echo "  docs/LOCAL_OPENAI_STUB.md"
echo "  vscode-extension/setup_continue.md"
echo "  vscode-extension/setup_aider.md"
echo ""
echo "Non-goals (by design in this portable repo):"
echo "  - no automatic multi‑GB weight download"
echo "  - no claim of replacing an IDE product end-to-end"
echo ""
echo "If you want a real local LM behind /v1, wire an external engine and keep receipts"
echo "aligned with docs/CLAIM_DISCIPLINE.md + docs/REPRO_BUNDLE_TEMPLATE.md."
echo ""

if command -v make >/dev/null 2>&1; then
  make standalone-openai-stub
  ./creation_os_openai_stub --self-test
fi

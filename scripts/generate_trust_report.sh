#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Emit a short human-readable verification report to stdout (redirect to docs/TRUST_REPORT.md).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "# Creation OS — trust / verification report (generated)"
echo
echo "Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo
echo "## Commands"
echo
echo "| Check | Meaning |"
echo "|---|---|"
echo "| \`make verify\` | Best-effort stack: Frama-C + extended sby + Hypothesis + integration slice |"
echo "| \`make red-team\` | v48 adversarial harness (Garak/DeepTeam SKIPs unless installed + model) |"
echo "| \`make merge-gate-v48\` | Optional heavy gate: verify + red-team + check-v31 + reviewer |"
echo "| \`make merge-gate\` | Portable flagship gate (v2 + v6..v29) |"
echo "| \`make formal-sby-v37\` | v37 σ-pipeline SymbiYosys (SKIP if \`sby\` missing) |"
echo
echo "## Tier map"
echo
echo "See \`docs/v47/INVARIANT_CHAIN.md\` and \`docs/WHAT_IS_REAL.md\` for M/T/P discipline."
echo
echo "## Quick probes"
echo

if command -v frama-c >/dev/null 2>&1; then
  echo "- **frama-c**: present (\`$(command -v frama-c)\`)"
else
  echo "- **frama-c**: absent (Layer 0 proofs SKIP in \`make verify-c\`)"
fi

if command -v sby >/dev/null 2>&1; then
  echo "- **sby**: present (\`$(command -v sby)\`)"
else
  echo "- **sby**: absent (Layer 1 formal SKIP in \`make verify-sv\`)"
fi

if python3 -c "import pytest, hypothesis" >/dev/null 2>&1; then
  echo "- **pytest+hypothesis**: present"
else
  echo "- **pytest+hypothesis**: absent (property layer SKIP in \`make verify-property\`)"
fi

if [[ -x ./creation_os_v47 ]]; then
  echo "- **creation_os_v47**: built"
else
  echo "- **creation_os_v47**: not built (\`make standalone-v47\`)"
fi

echo
echo "## Notes"
echo
echo "- v47 ZK API is a **stub** (see \`src/v47/zk_sigma.c\`): not zero-knowledge, not succinct soundness."
echo "- Frama-C/WP discharge is **tool-pinned**; treat \"all green\" as a release artifact, not a vibe."

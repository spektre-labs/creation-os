#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v142 merge-gate: Python SDK + OpenAI-SDK shape + adapter import smoke.
#
#   1. Python 3 is available.
#   2. `python3 -c "import creation_os"` works from a clean sys.path.
#   3. Full unittest suite passes (SDK shape parse + LangChain/LlamaIndex
#      adapter imports that don't require the frameworks installed).
#   4. pyproject.toml parses with stdlib tomllib (Python ≥ 3.11) or is
#      treated as SKIPPED on earlier versions — the file must exist.
#
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PYDIR="$ROOT/python"

echo "[v142] 1/4 python3 availability"
command -v python3 >/dev/null 2>&1 || {
    echo "[v142] FAIL python3 not on PATH"; exit 2; }
python3 --version

echo "[v142] 2/4 import smoke (stdlib-only)"
PYTHONPATH="$PYDIR" python3 - <<'PY'
import creation_os
from creation_os import COS, ChatResponse, ChatMessage, __version__
from creation_os.openai_compat import (
    sample_openai_response, sample_openai_headers, check_openai_drop_in
)
from creation_os.langchain_adapter import LANGCHAIN_AVAILABLE, require_langchain
from creation_os.llamaindex_adapter import LLAMAINDEX_AVAILABLE, CreationOSQueryEngine

cos = COS()
r = cos.parse_openai_response(sample_openai_response(), sample_openai_headers())
assert r.text == "Hello from Creation OS.", r.text
assert r.sigma_product is not None and abs(r.sigma_product - 0.231847) < 1e-4, r.sigma_product
assert r.specialist == "bitnet-2b-reasoning", r.specialist
assert r.emitted is True
print(f"  creation_os v{__version__} — OK")
print(f"  LANGCHAIN_AVAILABLE={LANGCHAIN_AVAILABLE}")
print(f"  LLAMAINDEX_AVAILABLE={LLAMAINDEX_AVAILABLE}")
ok, reason = check_openai_drop_in()
print(f"  openai drop-in: ok={ok} reason={reason}")
eng = CreationOSQueryEngine()
assert eng is not None
PY

echo "[v142] 3/4 unittest suite"
cd "$PYDIR"
PYTHONPATH="$PYDIR" python3 -m unittest discover -s tests -v 2>&1

echo "[v142] 4/4 pyproject.toml sanity"
[[ -f "$PYDIR/pyproject.toml" ]] || { echo "[v142] FAIL pyproject.toml missing"; exit 1; }
grep -q '^name *= *"creation-os"' "$PYDIR/pyproject.toml" \
    || { echo "[v142] FAIL pyproject.toml name"; exit 1; }

PYVER="$(python3 -c 'import sys; print(sys.version_info[:2] >= (3,11))')"
if [[ "$PYVER" == "True" ]]; then
  python3 - <<PY
import tomllib, pathlib
data = tomllib.loads(pathlib.Path("$PYDIR/pyproject.toml").read_text())
assert data["project"]["name"] == "creation-os"
assert "langchain" in data["project"]["optional-dependencies"]
assert "llamaindex" in data["project"]["optional-dependencies"]
assert "openai" in data["project"]["optional-dependencies"]
print("  pyproject.toml parsed OK (optional-deps: langchain, llamaindex, openai)")
PY
else
  echo "  pyproject.toml present (tomllib parse skipped on py<3.11)"
fi

echo "[v142] PASS — Python SDK + OpenAI shape + adapter imports"

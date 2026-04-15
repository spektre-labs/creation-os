#!/usr/bin/env bash
# Genesis — Zero-UI install (MLX + optional native M4 dispatcher). 1 = 1.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV="${ROOT}/.venv_genesis"
PY="${PYTHON:-python3}"

if ! command -v "${PY}" >/dev/null 2>&1; then
  echo "Python 3 not found. Install Python 3.10+ (Apple Silicon recommended)." >&2
  exit 1
fi

"${PY}" -m venv "${VENV}"
# shellcheck disable=SC1091
source "${VENV}/bin/activate"
python -m pip install -U pip wheel
python -m pip install -r "${ROOT}/mlx_creation_os/requirements.txt"
if [[ -f "${ROOT}/mlx_creation_os/requirements_benchmark.txt" ]]; then
  python -m pip install -r "${ROOT}/mlx_creation_os/requirements_benchmark.txt" || true
fi

if command -v xcrun >/dev/null 2>&1 && [[ -d "${ROOT}/creation_os/native_m4" ]]; then
  (cd "${ROOT}/creation_os/native_m4" && make full)
else
  echo "Skipping creation_os/native_m4 build (no Xcode / Metal SDK)."
fi

echo ""
echo "— Genesis env: source ${VENV}/bin/activate"
echo "— PYTHONPATH:  export PYTHONPATH=\"${ROOT}/mlx_creation_os:\${PYTHONPATH}\""
echo "— Phase 27:    CREATION_OS_PHASE27=1 python3 mlx_creation_os/benchmark_runner.py --limit 1"
echo "— E2E demo:    ./genesis_one_command_demo.sh \"What is 2+2?\""
echo "— MMLU micro:  cd mlx_creation_os && CREATION_OS_PHASE27=1 python3 standard_eval_runner.py mmlu --limit 5"
echo "Now."

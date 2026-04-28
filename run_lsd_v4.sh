#!/usr/bin/env bash
# σ-gate v4: clone Sirraya LSD, reproduce upstream (GPT-2), then adapt_lsd (Creation OS CSV).
# Set HF_TOKEN / HUGGING_FACE_HUB_TOKEN in your environment for gated models (never commit tokens).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
LOG_DIR="${ROOT}/benchmarks/sigma_gate_lsd/logs"
RES_DIR="${ROOT}/benchmarks/sigma_gate_lsd/results"
mkdir -p "${LOG_DIR}" "${RES_DIR}"

LOG="${LOG_DIR}/lsd_v4_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "${LOG}") 2>&1

echo "=== σ-GATE v4 (LSD) START: $(date -Iseconds) ==="
echo "ROOT=${ROOT}"

if [[ ! -d "${ROOT}/external/lsd" ]]; then
  echo "Cloning Sirraya LSD..."
  mkdir -p "${ROOT}/external"
  git clone --depth 1 https://github.com/sirraya-tech/Sirraya_LSD_Code.git "${ROOT}/external/lsd"
fi

PATCH="${ROOT}/benchmarks/sigma_probe_lsd/patches/sirraya_lsd_creation_os.patch"
if [[ -f "${PATCH}" ]] && ! grep -q "LSD_DEVICE_MAP" "${ROOT}/external/lsd/lsd/models/components.py" 2>/dev/null; then
  echo "Applying Creation OS vendor patch..."
  ( cd "${ROOT}/external/lsd" && patch -p0 < "${PATCH}" )
else
  echo "Vendor patch already applied or missing patch file."
fi

VENV="${ROOT}/benchmarks/sigma_gate_lsd/.venv"
PY="python3"
if [[ ! -x "${VENV}/bin/python" ]]; then
  echo "Creating venv at ${VENV} (PEP 668 safe)..."
  python3 -m venv "${VENV}"
fi
PY="${VENV}/bin/python"
echo "=== pip install (${PY}) ==="
"${PY}" -m pip install -q -r "${ROOT}/external/lsd/requirements.txt" \
  || "${PY}" -m pip install -q torch transformers sentence-transformers scikit-learn matplotlib datasets

export PYTHONPATH="${ROOT}/external/lsd${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

echo "=== Phase 3: upstream LSD (python -m lsd.main) ==="
cd "${ROOT}/external/lsd"
"${PY}" -m lsd.main
cd "${ROOT}"

RUN="${RES_DIR}/run_$(date +%Y%m%d_%H%M%S)"
echo "=== Phase 4: adapt_lsd.py → ${RUN} ==="
export CREATION_LSD_HF_MODEL="${CREATION_LSD_HF_MODEL:-gpt2}"
"${PY}" "${ROOT}/benchmarks/sigma_gate_lsd/adapt_lsd.py" \
  --prompts "${ROOT}/benchmarks/truthfulqa200/prompts.csv" \
  --output "${RUN}" \
  --hf-model "${CREATION_LSD_HF_MODEL}" \
  ${CREATION_LSD_LIMIT:+--limit ${CREATION_LSD_LIMIT}} \
  --epochs "${CREATION_LSD_EPOCHS:-8}"

ln -sfn "${RUN}" "${RES_DIR}/latest"
echo "Latest → ${RES_DIR}/latest"

echo "=== σ-GATE v4 COMPLETE: $(date -Iseconds) ==="

#!/usr/bin/env bash
# Sirraya LSD integration: clone + patch vendor, pip deps, upstream repro, Creation OS train.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
LOG_DIR="${ROOT}/benchmarks/sigma_probe_lsd/logs"
RES_DIR="${ROOT}/benchmarks/sigma_probe_lsd/results"
mkdir -p "${LOG_DIR}" "${RES_DIR}"

LOG="${LOG_DIR}/lsd_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "${LOG}") 2>&1

echo "=== LSD INTEGRATION START: $(date -Iseconds) ==="
echo "ROOT=${ROOT}"

if [[ ! -d "${ROOT}/external/lsd" ]]; then
  echo "Cloning Sirraya LSD..."
  mkdir -p "${ROOT}/external"
  git clone --depth 1 https://github.com/sirraya-tech/Sirraya_LSD_Code.git "${ROOT}/external/lsd"
fi

echo "Applying Creation OS vendor patch (device_map + LSD_NUM_PAIRS/LSD_EPOCHS)..."
if ! grep -q "LSD_DEVICE_MAP" "${ROOT}/external/lsd/lsd/models/components.py" 2>/dev/null; then
  ( cd "${ROOT}/external/lsd" && patch -p0 < "${ROOT}/benchmarks/sigma_probe_lsd/patches/sirraya_lsd_creation_os.patch" )
else
  echo "Vendor tree already patched."
fi

echo "=== pip install (requirements) ==="
python3 -m pip install -q -r "${ROOT}/external/lsd/requirements.txt" || python3 -m pip install -q torch transformers sentence-transformers scikit-learn matplotlib datasets

export PYTHONPATH="${ROOT}/external/lsd${PYTHONPATH:+:${PYTHONPATH}}"

echo "=== Phase 1: upstream LSD (GPT-2 + TruthfulQA + synthetic) ==="
# Optional quick smoke: LSD_NUM_PAIRS=200 LSD_EPOCHS=2
cd "${ROOT}/external/lsd"
python3 -m lsd.main
cd "${ROOT}"

echo "=== Phase 2: Creation OS TruthfulQA CSV + configurable HF model ==="
# Default GPT-2; set CREATION_LSD_HF_MODEL=google/gemma-2-2b-it and LSD_DEVICE_MAP=1 for Gemma on big RAM.
export CREATION_LSD_HF_MODEL="${CREATION_LSD_HF_MODEL:-gpt2}"
RUN_OUT="${RES_DIR}/creation_lsd_$(date +%Y%m%d_%H%M%S)"
python3 "${ROOT}/benchmarks/sigma_probe_lsd/train_creation_lsd.py" \
  --prompts "${ROOT}/benchmarks/truthfulqa200/prompts.csv" \
  --output "${RUN_OUT}" \
  --hf-model "${CREATION_LSD_HF_MODEL}" \
  ${CREATION_LSD_LIMIT:+--limit ${CREATION_LSD_LIMIT}} \
  --epochs "${CREATION_LSD_EPOCHS:-8}"

echo "Artifacts: ${RUN_OUT}"
echo "=== LSD INTEGRATION COMPLETE: $(date -Iseconds) ==="

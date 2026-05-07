#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# Generate CycloneDX JSON for the Python environment after a minimal editable
# install with optional dev dependencies (PyPI / supply-chain disclosure).
# Output: sbom/creation-os.cdx.json
#
# Requires: python3, pip (network on first cyclonedx-bom / creation-os install).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
VENVDIR="${ROOT}/.venv-sbom-cdx"
rm -rf "${VENVDIR}"
python3 -m venv "${VENVDIR}"
# shellcheck disable=SC1090
source "${VENVDIR}/bin/activate"
python -m pip install -q -U pip
python -m pip install -q cyclonedx-bom
python -m pip install -q -e ".[dev]"
mkdir -p sbom
cyclonedx-py environment \
  --pyproject pyproject.toml \
  --output-reproducible \
  --of JSON \
  -o sbom/creation-os.cdx.json \
  "${VENVDIR}/bin/python"
deactivate || true
rm -rf "${VENVDIR}"
echo "sbom-cdx: wrote sbom/creation-os.cdx.json"

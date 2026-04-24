#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Optional local STT for `cos voice`: build whisper.cpp with Metal (macOS)
# and fetch base.en (~150 MB). Does not vendor whisper.cpp into Creation OS.
#
#   bash scripts/real/setup_whisper.sh
#   cd "$COS_WHISPER_SRC" && ./server -m models/ggml-base.en.bin --host 127.0.0.1 --port 2022
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if command -v brew >/dev/null 2>&1; then
  brew install sdl2 || true
fi

COS_WHISPER_SRC="${COS_WHISPER_SRC:-$HOME/src/whisper.cpp}"
if [[ ! -d "$COS_WHISPER_SRC" ]]; then
  mkdir -p "$(dirname "$COS_WHISPER_SRC")"
  git clone https://github.com/ggerganov/whisper.cpp "$COS_WHISPER_SRC"
fi

cd "$COS_WHISPER_SRC"
git pull --ff-only 2>/dev/null || true

make clean 2>/dev/null || true
if [[ "$(uname -s)" == "Darwin" ]]; then
  WHISPER_METAL=1 make -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
  make -j"$(nproc 2>/dev/null || echo 4)"
fi

bash models/download-ggml-model.sh base.en

echo ""
echo "whisper.cpp ready at: $COS_WHISPER_SRC"
echo "Start HTTP server (cos voice default COS_WHISPER_URL):"
echo "  cd \"$COS_WHISPER_SRC\" && ./server -m models/ggml-base.en.bin --host 127.0.0.1 --port 2022"

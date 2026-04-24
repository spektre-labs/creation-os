#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# σ-separation benchmark against local Ollama (OpenAI-compatible
# POST /v1/chat/completions on port 11434). Uses the standard bitnet
# HTTP client so logprobs can populate σ when the Ollama build supports them.
#
# Prereq: `ollama serve` and the sigma-bench image:
#   ollama create sigma-bench -f data/models/Modelfile.sigma-bench
#
# Optional: COS_INFERENCE_BACKEND=ollama uses /api/chat; bitnet_server
# sends logprobs + enable_thinking:false in options and parses logprobs.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-sigma-bench}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
# Sampling defaults for OpenAI-compat /v1/chat/completions (sigma benchmark).
export COS_TEMPERATURE="${COS_TEMPERATURE:-0.7}"
export COS_TOP_P="${COS_TOP_P:-0.8}"
export COS_TOP_K="${COS_TOP_K:-20}"
# Ollama + logprobs + Codex system prompt can exceed the default 60 s socket budget.
export COS_BITNET_IO_TIMEOUT_S="${COS_BITNET_IO_TIMEOUT_S:-180}"
# Must exceed typical reasoning length so `message.content` fills (σ split).
export COS_BITNET_CHAT_MAX_TOKENS="${COS_BITNET_CHAT_MAX_TOKENS:-384}"
# Use native /api/chat so options.enable_thinking and defaults apply.
export COS_INFERENCE_BACKEND="${COS_INFERENCE_BACKEND:-ollama}"

exec bash scripts/real/sigma_separation.sh

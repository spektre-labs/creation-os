#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# σ-separation benchmark against local Ollama (OpenAI-compatible
# POST /v1/chat/completions on port 11434). Uses the standard bitnet
# HTTP client so logprobs can populate σ when the Ollama build supports them.
#
# Prereq: `ollama serve` and `ollama pull` for the chosen model.
# Do not set COS_INFERENCE_BACKEND=ollama for this script — that backend
# uses /api/chat and does not expose per-token logprobs to the parser.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-qwen3.5:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
# Ollama + logprobs + Codex system prompt can exceed the default 60 s socket budget.
export COS_BITNET_IO_TIMEOUT_S="${COS_BITNET_IO_TIMEOUT_S:-180}"
# logprobs JSON grows with max_tokens; keep modest for σ benchmark (stub_gen default is 4096).
export COS_BITNET_CHAT_MAX_TOKENS="${COS_BITNET_CHAT_MAX_TOKENS:-256}"

exec bash scripts/real/sigma_separation.sh

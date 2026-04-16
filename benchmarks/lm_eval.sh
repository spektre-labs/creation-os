#!/usr/bin/env bash
set -euo pipefail

# Optional harness hook for v28. This does not download datasets or weights.
# Install lm-eval in your venv, then export CREATION_OS_LM_EVAL_CMD to your runner.

if command -v lm_eval >/dev/null 2>&1; then
  echo "lm_eval: found (lm-eval-harness). Configure tasks/model outside this repo script."
  exit 0
fi

echo "lm_eval: SKIP (install https://github.com/EleutherAI/lm-evaluation-harness and ensure lm_eval is on PATH)"
exit 0

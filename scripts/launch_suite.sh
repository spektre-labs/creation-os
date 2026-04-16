#!/bin/sh
# Optional lab: OpenAI-shaped stub + static file server for suite_lab.html
# SPDX-License-Identifier: AGPL-3.0-or-later
set -e
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

STUB_PORT=${STUB_PORT:-8080}
HTTP_PORT=${HTTP_PORT:-3000}

if [ ! -x ./creation_os_openai_stub ]; then
  echo "Build stub first: make standalone-openai-stub" >&2
  exit 1
fi

if ! ./creation_os_openai_stub --self-test >/dev/null 2>&1; then
  echo "Stub self-test failed." >&2
  exit 1
fi

./creation_os_openai_stub --port "$STUB_PORT" &
STUB_PID=$!
trap 'kill "$STUB_PID" 2>/dev/null; exit' INT TERM EXIT

sleep 0.3
echo "Stub: http://127.0.0.1:${STUB_PORT}/v1/models (pid $STUB_PID)"
echo "Static: http://127.0.0.1:${HTTP_PORT}/suite_lab.html"
echo "Press Ctrl+C to stop."

if command -v python3 >/dev/null 2>&1; then
  ( cd web/static && python3 -m http.server "$HTTP_PORT" ) &
  HTTP_PID=$!
  trap 'kill "$STUB_PID" "$HTTP_PID" 2>/dev/null; exit' INT TERM EXIT
  sleep 0.3
  if command -v open >/dev/null 2>&1; then
    open "http://127.0.0.1:${HTTP_PORT}/suite_lab.html" || true
  elif command -v xdg-open >/dev/null 2>&1; then
    xdg-open "http://127.0.0.1:${HTTP_PORT}/suite_lab.html" || true
  fi
  wait "$HTTP_PID"
else
  echo "python3 not found; stub only. Open suite_lab.html via any static server on web/static." >&2
  wait "$STUB_PID"
fi

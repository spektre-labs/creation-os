#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
#
# v151: integration smoke for production image.
#   COS_DOCKER_SMOKE=compose  — full stack (sigma-gate + Ollama) via docker compose.
#   default / COS_DOCKER_SMOKE=single — build Dockerfile.prod, run one container, curl /v1/health.
#
# Requires Docker. Tears down on exit.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v docker >/dev/null 2>&1; then
  echo "check-docker-compose: SKIP (docker not on PATH)" >&2
  exit 0
fi

MODE="${COS_DOCKER_SMOKE:-single}"

single_smoke() {
  local tag=creation-os-smoke:tmp
  echo "check-docker-compose: docker build ($tag)..."
  docker build -f Dockerfile.prod -t "$tag" .

  local cid
  cid="$(docker run -d -p 3001:3001 "$tag")"
  cleanup_c() { docker rm -f "$cid" >/dev/null 2>&1 || true; }
  trap cleanup_c EXIT

  local ok=0
  for _ in $(seq 1 60); do
    if curl -fsS "http://127.0.0.1:3001/v1/health" | grep -q '"status":"ok"'; then
      ok=1
      break
    fi
    sleep 1
  done

  if [[ "$ok" != 1 ]]; then
    echo "check-docker-compose: FAIL (health)" >&2
    docker logs "$cid" >&2 || true
    return 1
  fi
  echo "check-docker-compose: OK ($(curl -fsS "http://127.0.0.1:3001/v1/health"))"
}

compose_smoke() {
  cleanup() {
    docker compose -f docker-compose.yml down -v --remove-orphans 2>/dev/null || true
  }
  trap cleanup EXIT

  echo "check-docker-compose: compose build..."
  docker compose -f docker-compose.yml build

  echo "check-docker-compose: compose up..."
  docker compose -f docker-compose.yml up -d

  local ok=0
  for _ in $(seq 1 90); do
    if curl -fsS "http://127.0.0.1:3001/v1/health" | grep -q '"status":"ok"'; then
      ok=1
      break
    fi
    sleep 2
  done

  if [[ "$ok" != 1 ]]; then
    echo "check-docker-compose: FAIL (health endpoint)" >&2
    docker compose -f docker-compose.yml logs --tail 80 sigma-gate >&2 || true
    exit 1
  fi

  echo "check-docker-compose: OK ($(curl -fsS "http://127.0.0.1:3001/v1/health"))"
}

if [[ "$MODE" == "compose" ]]; then
  compose_smoke
else
  single_smoke
fi
exit 0

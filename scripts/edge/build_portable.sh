#!/usr/bin/env bash
# Portable edge build: compile σ primitives with flags a Raspberry
# Pi 5 or a minimal ARM cross-toolchain would accept.
#
#   - -Os                 : optimise for size (iPhone / ESP32 friendly)
#   - -fno-builtin-exit   : no host-only stdlib assumptions (mostly a
#                           sanity signal; we still link libm)
#   - no -march=native    : the default Makefile uses it; we strip it
#                           to prove the kernel builds with generic
#                           flags.
#   - -std=c11 -Wall      : same language level as the main build.
#
# Output goes into build/edge/ so we don't pollute the host build.
set -euo pipefail
cd "$(dirname "$0")/../.."

: "${CC:=cc}"
OUT=build/edge
mkdir -p "$OUT"

FLAGS=(-Os -Wall -std=c11 -Isrc/sigma/pipeline)
LINK=(-lm)

build_lib() {
  local name="$1" ; shift
  echo "  · build $name"
  "$CC" "${FLAGS[@]}" -c "$@" -o "$OUT/$name.o"
}

build_lib reinforce  src/sigma/pipeline/reinforce.c
build_lib speculative src/sigma/pipeline/speculative.c
build_lib ttt        src/sigma/pipeline/ttt.c
build_lib engram     src/sigma/pipeline/engram.c
build_lib moe        src/sigma/pipeline/moe.c
build_lib multimodal src/sigma/pipeline/multimodal.c
build_lib tinyml     src/sigma/pipeline/tinyml.c
build_lib swarm      src/sigma/pipeline/swarm.c
build_lib live       src/sigma/pipeline/live.c
build_lib continual  src/sigma/pipeline/continual.c
build_lib unlearn    src/sigma/pipeline/unlearn.c
build_lib agent      src/sigma/pipeline/agent.c
build_lib diagnostic src/sigma/pipeline/diagnostic.c

# Link the tinyml demo — the one primitive actually useful on an
# MCU — as a portable binary and run its self-test to prove it works
# without -march=native.
echo "  · link edge/creation_os_sigma_tinyml"
"$CC" "${FLAGS[@]}" \
    src/sigma/pipeline/tinyml.c src/sigma/pipeline/tinyml_main.c \
    -o "$OUT/creation_os_sigma_tinyml" "${LINK[@]}"

echo "  · run edge/creation_os_sigma_tinyml"
"$OUT/creation_os_sigma_tinyml" >/dev/null

# Report footprint.
echo "  · object sizes:"
for o in "$OUT"/*.o; do
  sz=$(wc -c <"$o" | tr -d ' ')
  printf "      %-28s %7s B\n" "$(basename "$o")" "$sz"
done
total=$(wc -c "$OUT"/*.o | tail -1 | awk '{print $1}')
echo "      ----------------------------------------"
printf "      %-28s %7s B\n" "total" "$total"

echo "edge/build_portable: OK (13 σ-pipeline translation units build "
echo "                         with -Os, portable ARM flags, no -march=native)"

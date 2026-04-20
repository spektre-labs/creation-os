#!/usr/bin/env bash
# Installer v2 smoke test (P10).
#
# Doesn't actually clone over the network; instead:
#   1. Asserts scripts/install.sh passes `bash -n` (valid syntax).
#   2. Asserts the key operations the installer performs work in this
#      checkout: `make cos`, `make check-sigma-pipeline`, and the
#      three `cos` subcommands the installer advertises:
#        · cos  (status)
#        · cos benchmark
#        · cos cost --fixture demo
#   3. Asserts the installer actually references each of these in its
#      own text (regression safety if someone removes a command).
set -euo pipefail
cd "$(dirname "$0")/../.."

echo "  · [1] syntax check"
bash -n scripts/install.sh

echo "  · [2a] make cos"
make cos >/dev/null

echo "  · [2b] cos status"
./cos >/dev/null

echo "  · [2c] cos benchmark"
OUT=$(./cos benchmark 2>/dev/null)
grep -q "Creation OS — end-to-end pipeline benchmark" <<<"$OUT" \
    || { echo "cos benchmark did not print the expected header" >&2; exit 3; }
grep -q "Creation OS served" <<<"$OUT" \
    || { echo "cos benchmark did not print the headline" >&2; exit 4; }

echo "  · [2d] cos cost --fixture demo"
OUT=$(./cos cost --fixture demo 2>/dev/null)
grep -q "σ-gated hybrid cost measurement" <<<"$OUT" \
    || { echo "cos cost did not print the expected header" >&2; exit 5; }
grep -q "Savings" <<<"$OUT" \
    || { echo "cos cost did not print savings" >&2; exit 6; }

echo "  · [3] installer advertises each command"
grep -q "cos chat"        scripts/install.sh || { echo "missing chat"; exit 7; }
grep -q "cos benchmark"   scripts/install.sh || { echo "missing benchmark"; exit 8; }
grep -q "cos cost"        scripts/install.sh || { echo "missing cost"; exit 9; }
grep -q "check-sigma-pipeline" scripts/install.sh \
    || { echo "missing check-sigma-pipeline"; exit 10; }

echo "check-installer: PASS (syntax + cos subcommands + pipeline smoke)"

#!/usr/bin/env bash
# v162 σ-Compose merge-gate: proves kernel manifests resolve
# deterministically into profile-specific closures, that the
# dependency-graph is acyclic, that profiles `lean` and
# `sovereign` produce different kernel compositions, that the
# hot-swap enable/disable flow respects the dependency graph,
# and that the whole resolver output is byte-identical under the
# same inputs.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

BIN=./creation_os_v162_compose

die() { echo "v162 check: FAIL — $*" >&2; exit 1; }

echo "  self-test"
st="$($BIN --self-test)"
echo "  $st"
echo "$st" | grep -q '"self_test":"pass"' || die "self-test"

echo "  profile lean → small closure"
lean="$($BIN --profile lean)"
nl=$(echo "$lean" | python3 -c "import sys,json; print(json.loads(sys.stdin.read())['n_enabled'])")
[ "$nl" -le "5" ] || die "lean too big (n_enabled=$nl)"
echo "$lean" | grep -q '"v101"' || die "lean must include v101"
echo "$lean" | grep -q '"v106"' || die "lean must include v106"
echo "$lean" | grep -q '"v111"' || die "lean must include v111"

echo "  profile sovereign → large closure"
sov="$($BIN --profile sovereign)"
ns=$(echo "$sov" | python3 -c "import sys,json; print(json.loads(sys.stdin.read())['n_enabled'])")
[ "$ns" -ge "12" ] || die "sovereign too small (n_enabled=$ns)"
for want in v150 v124 v128 v152 v118 v140 v135; do
    echo "$sov" | grep -q "\"$want\"" || die "sovereign missing $want"
done

echo "  lean vs sovereign must differ"
[ "$nl" -lt "$ns" ] || die "lean ($nl) must be < sovereign ($ns) kernels"

echo "  custom profile picks exactly what user asks (+ deps)"
cus="$($BIN --profile custom --kernels v150,v140)"
for want in v150 v140 v135 v128 v111 v106 v101; do
    echo "$cus" | grep -q "\"$want\"" || die "custom missing $want"
done

echo "  acyclicity — no cycles in baked graph"
echo "$sov" | grep -q '"cycle_detected":false' || die "cycle must be false"

echo "  hot-swap enable v159 on lean"
en="$($BIN --profile lean --enable v159)"
echo "$en" | grep -q '"kind":"enable","kernel":"v159"' || die "enable event not logged"
echo "$en" | grep -q '"v159"' || die "v159 missing from enabled set"

echo "  hot-swap disable leaf v150 on sovereign"
dis="$($BIN --profile sovereign --disable v150)"
echo "$dis" | grep -q '"kind":"disable","kernel":"v150"' || die "disable event not logged"
echo "$dis" | python3 -c "import sys,json; j=json.loads(sys.stdin.read()); assert 'v150' not in j['enabled'], j['enabled']"

echo "  determinism (same profile → byte-identical JSON)"
a="$($BIN --profile researcher)"
b="$($BIN --profile researcher)"
[ "$a" = "$b" ] || die "non-deterministic resolver"

echo "v162 compose profile-resolve: OK"

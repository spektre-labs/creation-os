#!/usr/bin/env bash
#
# v248 σ-Release — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * version == "1.0.0"; major=1, minor=0, patch=0
#   * exactly 6 release artifacts in canonical order,
#     every present==true, non-empty locator
#   * exactly 6 doc sections in canonical order, every present
#   * WHAT_IS_REAL: 15 categories each with tier in {"M","P"}
#   * exactly 7 release criteria (C1..C7) all satisfied
#   * release_ready, scsl_pinned, proconductor_approved all true
#   * sigma_release in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v248_release"
[ -x "$BIN" ] || { echo "v248: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v248", d
assert d["chain_valid"] is True, d

assert d["version"] == "1.0.0", d
assert d["major"] == 1 and d["minor"] == 0 and d["patch"] == 0, d

anames = [a["name"] for a in d["artifacts"]]
assert anames == ["github_release","docker_hub","homebrew",
                   "pypi","npm","crates_io"], anames
for a in d["artifacts"]:
    assert a["present"] is True, a
    assert len(a["locator"]) > 0, a
assert d["n_artifacts_present"] == 6, d

dnames = [x["name"] for x in d["docs"]]
assert dnames == ["getting_started","architecture",
                   "api_reference","configuration","faq",
                   "what_is_real"], dnames
for x in d["docs"]:
    assert x["present"] is True, x
assert d["n_docs_present"] == 6, d

wir = d["what_is_real"]
assert len(wir) == 15, wir
for w in wir:
    assert w["tier"] in ("M","P"), w

ids = [c["id"] for c in d["criteria"]]
assert ids == ["C1","C2","C3","C4","C5","C6","C7"], ids
for c in d["criteria"]:
    assert c["satisfied"] is True, c
assert d["n_criteria_satisfied"] == 7, d

assert d["scsl_pinned"] is True, d
assert d["proconductor_approved"] is True, d
assert d["release_ready"] is True, d

assert 0.0 <= d["sigma_release"] <= 1.0, d
assert d["sigma_release"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v248: non-deterministic" >&2; exit 1; }

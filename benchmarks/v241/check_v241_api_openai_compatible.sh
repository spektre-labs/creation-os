#!/usr/bin/env bash
#
# v241 σ-API-Surface — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 10 endpoints, every path starts with /v1/,
#     every method in {GET, POST}, every endpoint emits
#     X-COS-* headers
#   * exactly one OpenAI-compatible endpoint: POST /v1/chat/completions
#   * exactly 4 SDKs named python, javascript, rust, go (in order)
#     all maintained, install string non-empty
#   * api_version_major == 1
#   * sigma_api in [0,1] and == 0.0 (every endpoint passed audit)
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v241_api"
[ -x "$BIN" ] || { echo "v241: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v241", d
assert d["chain_valid"] is True, d
assert d["n_endpoints"] == 10, d
assert d["api_version_major"] == 1, d
assert d["api_version_minor"] >= 0, d

compat = [e for e in d["endpoints"] if e["openai_compatible"]]
assert len(compat) == 1, compat
assert compat[0]["method"] == "POST", compat
assert compat[0]["path"]   == "/v1/chat/completions", compat

methods_seen = set()
for e in d["endpoints"]:
    assert e["method"] in ("GET", "POST"), e
    assert e["path"].startswith("/v1/"),    e
    assert e["emits_cos_headers"] is True,  e
    assert e["audit_ok"]          is True,  e
    methods_seen.add(e["method"])
assert methods_seen == {"GET", "POST"}, methods_seen

wanted = ["/v1/chat/completions", "/v1/reason", "/v1/plan",
          "/v1/create", "/v1/simulate", "/v1/teach",
          "/v1/health", "/v1/identity", "/v1/coherence",
          "/v1/audit/stream"]
paths = [e["path"] for e in d["endpoints"]]
for w in wanted:
    assert w in paths, (w, paths)

assert d["n_sdks"] == 4, d
assert d["n_sdks_maintained"] == 4, d
names = [s["name"] for s in d["sdks"]]
assert names == ["python", "javascript", "rust", "go"], names
for s in d["sdks"]:
    assert s["maintained"] is True, s
    assert len(s["install"]) > 0, s

assert 0.0 <= d["sigma_api"] <= 1.0, d
assert d["sigma_api"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v241: non-deterministic" >&2; exit 1; }

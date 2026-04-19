#!/usr/bin/env bash
#
# v257 σ-Locale — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 10 locales in canonical order
#     (en, fi, zh, ja, de, fr, es, ar, hi, pt), every
#     timezone + date_format non-empty, currency is a
#     3-letter ISO code, sigma_language in [0,1],
#     locale_ok
#   * exactly 3 cultural styles in canonical order
#     (direct, indirect, high_context); every style_ok;
#     example_locale is one of the 10 declared locales
#   * exactly 3 legal regimes in canonical order
#     (EU_AI_ACT, GDPR, COLORADO_AI_ACT); every
#     compliant==true AND last_reviewed>0
#   * exactly 2 time checks; every pass==true
#   * sigma_locale in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v257_locale"
[ -x "$BIN" ] || { echo "v257: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json, re
d = json.loads("""$OUT""")
assert d["kernel"] == "v257", d
assert d["chain_valid"] is True, d

ln = [x["locale"] for x in d["locales"]]
want = ["en","fi","zh","ja","de","fr","es","ar","hi","pt"]
assert ln == want, ln
for l in d["locales"]:
    assert len(l["timezone"])    > 0, l
    assert len(l["date_format"]) > 0, l
    assert re.fullmatch(r"[A-Z]{3}", l["currency"]), l
    assert 0.0 <= l["sigma_language"] <= 1.0, l
    assert l["locale_ok"] is True, l
assert d["n_locales_ok"] == 10, d

declared = set(ln)
sn = [s["style"] for s in d["styles"]]
assert sn == ["direct","indirect","high_context"], sn
for s in d["styles"]:
    assert s["example_locale"] in declared, s
    assert s["style_ok"] is True, s
assert d["n_styles_ok"] == 3, d

rn = [r["regime"] for r in d["regimes"]]
assert rn == ["EU_AI_ACT","GDPR","COLORADO_AI_ACT"], rn
for r in d["regimes"]:
    assert r["compliant"] is True, r
    assert r["last_reviewed"] > 0, r
assert d["n_regimes_ok"] == 3, d

for t in d["time"]:
    assert t["pass"] is True, t
assert d["n_time_ok"] == 2, d

assert 0.0 <= d["sigma_locale"] <= 1.0, d
assert d["sigma_locale"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v257: non-deterministic" >&2; exit 1; }

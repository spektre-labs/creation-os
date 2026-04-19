#!/usr/bin/env bash
#
# v246 σ-Harden — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 5 chaos scenarios in canonical order
#     (kill-random-kernel, high-load, network-partition,
#      corrupt-memory, oom-attempt) all recovered with
#     error_path_taken==true and recovery_ticks > 0
#   * exactly 6 resource limits in canonical order,
#     every value > 0
#   * exactly 5 input-validation checks, all pass
#   * exactly 5 security items, all on
#   * sigma_harden in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v246_harden"
[ -x "$BIN" ] || { echo "v246: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v246", d
assert d["chain_valid"] is True, d
assert d["n_chaos"] == 5, d
assert d["n_limits"] == 6, d
assert d["n_input_checks"] == 5, d
assert d["n_security"] == 5, d

chaos_names = [c["name"] for c in d["chaos"]]
assert chaos_names == ["kill-random-kernel","high-load",
                        "network-partition","corrupt-memory",
                        "oom-attempt"], chaos_names
for c in d["chaos"]:
    assert c["error_path_taken"] is True, c
    assert c["recovered"] is True, c
    assert c["recovery_ticks"] > 0, c
assert d["n_recovered"] == 5, d

lim_names = [l["name"] for l in d["limits"]]
assert lim_names == ["max_tokens_per_request",
                      "max_time_ms_per_request",
                      "max_kernels_per_request",
                      "max_memory_mb_per_session",
                      "max_disk_mb_per_session",
                      "max_concurrent_requests"], lim_names
for l in d["limits"]:
    assert l["value"] > 0, l
assert d["n_limits_positive"] == 6, d

in_names = [i["name"] for i in d["inputs"]]
assert in_names == ["max_prompt_length","utf8_encoding_ok",
                     "injection_filter_ok","rate_limit_ok",
                     "auth_token_required"], in_names
for i in d["inputs"]:
    assert i["pass"] is True, i
assert d["n_inputs_passing"] == 5, d

sec_names = [s["name"] for s in d["security"]]
assert sec_names == ["tls_required","auth_token_required",
                      "audit_log_on","containment_on",
                      "scsl_license_pinned"], sec_names
for s in d["security"]:
    assert s["on"] is True, s
assert d["n_security_on"] == 5, d

assert 0.0 <= d["sigma_harden"] <= 1.0, d
assert d["sigma_harden"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v246: non-deterministic" >&2; exit 1; }

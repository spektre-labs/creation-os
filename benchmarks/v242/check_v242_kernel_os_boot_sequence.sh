#!/usr/bin/env bash
#
# v242 σ-Kernel-OS — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_processes == 12 with unique priorities forming
#     [0, 12); priority == argsort sigma ascending
#   * exactly 3 IPC mechanisms named MESSAGE_PASSING,
#     SHARED_MEMORY, SIGNALS (in order)
#   * exactly 5 FS dirs named models, memory, config, audit,
#     skills (in order), each path prefixed ~/.creation-os/
#   * boot sequence: 6 steps with kernel_id order
#     29 -> 101 -> 106 -> 234 -> 162 -> 239 (all ok)
#   * >= 5 kernels ready after boot including {29,101,106,234,162}
#   * shutdown: 3 steps with kernel_id order 231 -> 181 -> 233
#   * sigma_os in [0,1] AND == 0.0; state == STOPPED
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v242_kernel_os"
[ -x "$BIN" ] || { echo "v242: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v242", d
assert d["chain_valid"] is True, d
assert d["n_processes"] == 12, d
assert d["n_ipc"]       == 3,  d
assert d["n_fs_dirs"]   == 5,  d
assert d["n_boot_steps"]     == 6, d
assert d["n_shutdown_steps"] == 3, d

procs = d["processes"]
prios = [p["priority"] for p in procs]
assert sorted(prios) == list(range(12)), prios
sigmas = [p["sigma"] for p in procs]
for i in range(1, len(sigmas)):
    assert sigmas[i] >= sigmas[i-1] - 1e-6, sigmas

ipc_names = [c["name"] for c in d["ipc"]]
assert ipc_names == ["MESSAGE_PASSING", "SHARED_MEMORY", "SIGNALS"], ipc_names

dir_names = [x["name"] for x in d["fs_dirs"]]
assert dir_names == ["models", "memory", "config", "audit", "skills"], dir_names
for x in d["fs_dirs"]:
    assert x["path"].startswith("~/.creation-os/"), x

boot_ids = [b["kernel_id"] for b in d["boot"]]
assert boot_ids == [29, 101, 106, 234, 162, 239], boot_ids
for i, b in enumerate(d["boot"], start=1):
    assert b["step"] == i, b
    assert b["ok"] is True, b

assert d["n_ready_kernels"] >= 5, d
need = {29, 101, 106, 234, 162}
ready_ids = {p["kernel_id"] for p in procs if p["ready"]}
assert need.issubset(ready_ids), (need, ready_ids)

shut_ids = [b["kernel_id"] for b in d["shutdown"]]
assert shut_ids == [231, 181, 233], shut_ids
for i, b in enumerate(d["shutdown"], start=1):
    assert b["step"] == i, b
    assert b["ok"] is True, b

assert 0.0 <= d["sigma_os"] <= 1.0, d
assert d["sigma_os"] < 1e-6, d
assert d["state"] == "STOPPED", d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v242: non-deterministic" >&2; exit 1; }

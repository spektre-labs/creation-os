# Software Verification Plan (SVP) — σ-critical path (v49 lab)

## Verification methods

| Method | Artifact / command | Notes |
|---|---|---|
| Requirements-based tests | `tests/v49/mcdc_tests.c` | Deterministic, no network |
| Structural coverage | `scripts/v49/run_mcdc.sh` | gcov/lcov best-effort |
| Formal analysis (DO-333 style) | `frama-c -wp -wp-rte` on `src/v47/sigma_kernel_verified.c` | SKIPs if tool absent |
| HDL bounded proof | `make formal-sby-v47` | Separate layer from C kernel |
| Traceability automation | `python3 scripts/v49/verify_traceability.py` | Manifest-driven |
| Binary hygiene | `scripts/v49/binary_audit.sh` | Not a forensic certification |
| Red team harness | `make red-team` | v48 catalog + pytest |

## Independence

Independent authority review is **not** claimed by this repository. External audit is explicitly out-of-band.

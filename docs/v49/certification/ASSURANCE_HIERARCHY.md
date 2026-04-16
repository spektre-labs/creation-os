# Assurance hierarchy (Creation OS — v49 lab framing)

This is a **communication ladder** for what the repository actually tries to verify. It is **not** a regulatory certification stack.

```text
ASSURANCE LEVEL 6: ZK cryptographic proof (v47 stub architecture)
  “commit + verify σ” — P-tier until a real proof system exists

ASSURANCE LEVEL 5: Red team harness (v48)
  “attack catalog + pytest + optional external scanners” — SKIPs explicit

ASSURANCE LEVEL 4: DO-178C-aligned plans + requirements pack (v49)
  “PSAC/SVP/HLR/LLR/traceability” — documentation discipline, not FAA approval

ASSURANCE LEVEL 3: Binary / sanitizer hygiene scripts (v49)
  “nm/strings/ASan/optional valgrind” — lab forensics, not Cellebrite certification

ASSURANCE LEVEL 2: Frama-C / WP targets (v47)
  “annotated obligations” — M-tier only with discharged proof logs

ASSURANCE LEVEL 1: SymbiYosys FPGA proofs (v37/v47 harness)
  “bounded SVA/BMC” — M-tier when `sby` is green

ASSURANCE LEVEL 0: Portable merge gate (v2 + v6..v29)
  “build + flagship self-tests” — M-tier in `WHAT_IS_REAL.md`
```

## One command

`make certify` runs the **best-effort** automation across these layers (with SKIPs).

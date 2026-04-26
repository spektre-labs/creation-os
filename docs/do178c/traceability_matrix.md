# DO-178C-oriented traceability (informative)

**Read first:** [../CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md). This folder maps common **DO-178C / DO-333 vocabulary** to **in-tree artifacts**. It does **not** state that Creation OS has been certified under DO-178C, any Design Assurance Level (DAL), or any aviation authority process. It is **engineering traceability scaffolding** for maintainers and auditors who already understand those standards.

**Bidirectional traceability** (requirements ↔ design ↔ code ↔ verification) is approximated here by:

| DO-178C life-cycle idea | Creation OS pointer | Typical evidence |
|-------------------------|---------------------|------------------|
| High-level / system intent | Codex + constitution + benchmarks | `src/sigma/pipeline/codex.c`, `src/sigma/constitution.c`, `benchmarks/` |
| Software requirements (behavior) | σ-gate contracts, pipeline invariants | `src/sigma/pipeline/reinforce.*`, `src/v138/sigma_gate.c`, `specs/v138/sigma_gate.acsl` |
| Software design (interfaces) | Public headers | `src/sigma/pipeline/*.h`, `src/sigma/proof_receipt.h` |
| Coding standard | Toolchain flags | `Makefile` (`-std=c11`, `-Wall`, …) |
| Source code | C implementation | `src/**/*.c` |
| Integration | CLI + pipeline smoke | `benchmarks/sigma_pipeline/check_cos_chat.sh`, `check-sigma-pipeline` |
| Software verification (tests) | `make check-*` matrix | `Makefile`, `tests/` |
| Formal methods supplement (DO-333) | Lean / Frama-C where present | `benchmarks/sigma_pipeline/check_lean_t3_discharged.sh`, `benchmarks/v138/check_v138_prove_frama_c_wp.sh`, `hw/formal/v259/*.lean` |
| Traceability matrix (this effort) | CSV + this document | [trace.csv](trace.csv) |
| Configuration management | Git + tagged releases | `git` history; optional SHA-256 proof receipts (`src/sigma/proof_receipt.c`) |

## One-command formal / trace rollup

The Makefile target **`verify-do178c`** runs trace CSV validation, the in-repo Lean discharge check (`check-lean-t3-discharged`), the v138 σ-proof harness (tier-1 ACSL validation plus opportunistic Frama-C), and optional receipt-chain audit files when present. It is **orthogonal** to the existing top-level **`make verify`** target (RTL / Frama-C on `sigma_kernel_verified.c` and integration self-tests).

## Evidence bundle (local)

Use **`cos certify --output <dir>`** (optional **`--dal-a`** flag accepted as a label only) to emit a **local** evidence folder with planning-style markdown and copies of trace files. That output is **not** a Plan for Software Aspects of Certification (PSAC) or other certification deliverable unless your organization completes the full process outside this repository.

## WCET

See [wcet_notes.md](wcet_notes.md). The repository does not claim measured worst-case execution time (WCET) certificates for production deployments.

# Software Development Plan (SDP) — σ-critical path (v49 lab)

## Lifecycle model

Use **Git** as the authoritative configuration management record (see `SCMP.md`).

## Coding standard

- **Language:** ISO C11 (`-std=c11`)
- **Warnings:** `-Wall -Wextra` on lab targets; stricter single-TU checks in `binary_audit.sh`
- **Memory:** σ-kernel path avoids heap allocation in `src/v47/sigma_kernel_verified.c` (see trace checks)

## Reviews

- Maintainer review for changes touching `src/v47/sigma_kernel_verified.c`, `src/v49/sigma_gate.c`, or certification docs.

## Build / release

There is **no aviation “release artifact”** here. The closest “release-like” command is `make certify` (best-effort, SKIPs allowed).

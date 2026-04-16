# Creation OS v61 — Σ-Citadel

> The first open-source AI-agent runtime to ship the full
> **DARPA-CHACE advanced-security menu** as one `make chace` gate.

## What v61 is

`v61` assembles every state-of-the-art defence primitive the 2026
landscape offers for a local AI agent into one named artefact with
one verification command.  Nothing is claimed that is not run.

| Primitive                | Where in v61                            | Tier |
|--------------------------|-----------------------------------------|------|
| seL4 microkernel         | `sel4/sigma_shield.camkes` (contract)   | I    |
| Wasmtime sandbox         | `wasm/example_tool.c` + harness         | I    |
| eBPF LSM policy          | `ebpf/sigma_shield.bpf.c`               | I    |
| Bell-LaPadula (conf.)    | `cos_v61_lattice_check`                 | **M** |
| Biba (integrity)         | `cos_v61_lattice_check`                 | **M** |
| MLS compartments         | `cos_v61_label_t.compartments`          | **M** |
| Attestation quote        | `cos_v61_quote256` (libsodium opt-in)   | **M** |
| v60 composition          | `cos_v61_compose`                       | **M** |
| Sigstore / cosign        | `scripts/security/sign.sh`              | I    |
| libsodium BLAKE2b        | `COS_V61_LIBSODIUM=1`                   | I    |
| Nix reproducible build   | `nix/v61.nix`                           | I    |
| Distroless runtime       | `Dockerfile.distroless`                 | I    |
| SLSA-3 provenance        | `.github/workflows/slsa.yml`            | P    |
| Darwin sandbox-exec      | `sandbox/darwin.sb`                     | I    |
| OpenBSD pledge/unveil    | `sandbox/openbsd_pledge.c`              | I    |
| CHERI hw caps            | (documented target; no M4 hw)           | P    |

**M** = runtime-checked deterministic invariant (this repo).
**I** = interpreted / documented contract.  **P** = planned.

## Why ship all of them together

2026 research is unambiguous: no single layer closes the agent attack
surface.  DDIPE (arXiv 2604.03081) sidesteps prompt-injection filters;
ClawWorm (arXiv 2603.15727) sidesteps skill-registry vetting; the
Malicious Intermediary attack (arXiv 2604.08407) sidesteps tool
allowlists.  Every successful attack works by **ambiguous payload**.

`v60` σ-Shield refuses α-dominated intent.  `v61` Σ-Citadel layers
**mandatory confidentiality + integrity labels** on top so that even
a decision that slips σ cannot cross a lattice boundary.  Together
the two kernels are the narrowest-possible authorisation gate that
still admits legitimate work.

## Running it

```
make check-v61          # ≥60 lattice + attestation self-tests
make microbench-v61     # 6 × 10⁸ lattice decisions/s on M4
make chace              # every layer of the DARPA-CHACE menu, SKIPping honestly
make attest             # emit ATTESTATION.json (quote over code/caps/σ/lattice/nonce)
make slsa > PROVENANCE.json
make distroless         # build gcr.io/distroless/cc-debian12 runtime
```

The `chace` aggregator prints `PASS` / `SKIP (why)` / `FAIL` per layer,
never silently downgrading a missing tool to PASS.

## Non-claims

* v61 does **not** replace seL4 / Wasmtime / gVisor / Firecracker —
  it composes with them via documented contracts.
* The deterministic XOR-fold quote is **not** a cryptographic MAC.
  Enable `COS_V61_LIBSODIUM=1` for BLAKE2b-256 (`crypto_generichash`).
* TPM 2.0 / Apple Secure Enclave binding is **P-tier**; `v61` can
  take a hardware-computed hash as input but does not itself issue
  the quote over a TPM today.
* Formal proof of the lattice is **not** shipped yet (planned via
  Frama-C ACSL + TLA+ under `v47` / `v49`).  All current v61 claims
  are runtime-checked **M**.

## See also

* `docs/v61/ARCHITECTURE.md` — per-surface wiring, tier map, measured
  perf, threat-model tie-ins.
* `docs/v61/POSITIONING.md` — v61 vs DARPA CHACE, IronClaw, seL4-only,
  Fuchsia, Claude Code.
* `docs/v61/paper_draft.md` — problem, design, proofs, numbers,
  limitations.

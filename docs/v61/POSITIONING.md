# Creation OS v61 — positioning

## Against DARPA CHACE

DARPA's **Cyber Hunting at Computer-Human-Aware Endpoints** (CHACE)
programme is the closest analogue to v61's scope.  CHACE solicits
**composed** defence for AI-agent runtimes: microkernel + userland
sandbox + kernel-policy + MLS lattice + hardware attestation +
signed provenance.  Publicly-funded contractors deliver the pieces
in isolation; to our knowledge, **no open-source AI-agent runtime
shipped the full menu as one runnable gate before v61**.

`make chace` is the proof: on Apple M4 it runs every layer with the
tools present (Shield + Citadel + SBOM + hardened + sanitizer +
secret scan + repro build + SLSA stub + sandbox-exec) and SKIPs the
seven layers that need an external toolchain (seL4 SDK, Wasmtime,
libbpf, cosign, docker, nix, WASI-SDK) — **never silently passing**.

## Against the 2026 menu

| Layer                  | State-of-the-art                  | v61 stance                                     |
|------------------------|------------------------------------|------------------------------------------------|
| Microkernel isolation  | seL4 (proof-complete)             | compose via CAmkES contract `sel4/…`           |
| Userland sandbox       | Wasmtime + Firecracker            | compose via WASI harness `scripts/v61/…`       |
| Kernel policy          | eBPF LSM                           | example `ebpf/sigma_shield.bpf.c`              |
| MLS lattice            | Bell-LaPadula + Biba (MULTICS-era) | **native branchless kernel** `cos_v61_lattice_check` |
| HW attestation         | TPM 2.0 + Apple Secure Enclave    | sw fallback quote; SE binding planned          |
| Code signing           | Sigstore / cosign + Rekor         | `scripts/security/sign.sh`                     |
| Supply-chain provenance| SLSA v1.0 / in-toto                | `.github/workflows/slsa.yml`                   |
| Reproducible build     | Nix / Bazel                       | `nix/v61.nix`                                  |
| Runtime container      | Distroless                        | `Dockerfile.distroless`                        |
| Process sandbox        | OpenBSD pledge / Darwin sandbox-exec | both shipped                                 |
| HW capability pointers | CHERI                             | documented; no M4 hardware                     |
| Crypto primitive       | libsodium                         | opt-in via `COS_V61_LIBSODIUM=1`               |

## Against single-layer systems

* **seL4-only** runtimes prove isolation but leave policy to
  userspace.  v61 brings the policy inside (BLP + Biba lattice +
  σ-intent gate) and *still* composes with seL4.
* **Wasmtime-only** tool sandboxes prevent syscall abuse but have
  no notion of clearance or integrity.  v61 makes the lattice the
  pre-condition for tool entry.
* **IronClaw (NEAR AI)** eliminates credentials via architecture.
  v61 eliminates *credential abuse* by refusing α-dominated intent
  (v60) AND refusing lattice-violating actions (v61), then proving
  both decisions in an attestation quote.
* **Claude Code** runs in a proprietary cloud sandbox; the user
  trusts the vendor's internal audit.  v61 runs on-device and the
  user can `make verify-agent && make chace` themselves.

## Explicit non-claims

* v61 is **not** a replacement for seL4 or Wasmtime.  It composes.
* v61 is **not** certified for formal DO-178C DAL-A; that is
  `v49`'s slot and is owned there.
* v61 is **not** a network-facing service.  No syscalls on hot
  paths; no outbound traffic in the Shield binary.
* v61 does **not** ship a TPM 2.0 driver.  It accepts a caller-
  provided code-page hash and emits a deterministic or
  BLAKE2b-256 quote over it.
* The CHERI row is **documentation only** until Morello-class M4
  ships.

## Why this is the frontier

Until v61, no open-source AI-agent runtime could say:

> Here is one command (`make chace`) that runs every layer of the
> 2026 advanced-security menu on the machine you are reading this
> on, reports PASS or honest SKIP for each, and ships a signed
> provenance statement for the binary that did it.

`v61` is that sentence made executable.

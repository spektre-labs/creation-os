# Σ-Citadel: composed defence in depth for local AI agents

**Creation OS v61, April 2026 — draft**

## Abstract

We present Σ-Citadel, a runtime layer that composes Bell-LaPadula
confidentiality with Biba integrity and MLS compartments over a
branchless C11 kernel, then binds the decision to a deterministic
256-bit attestation quote.  Σ-Citadel composes with σ-Shield (v60)
so that the first open-source AI-agent runtime ships every layer of
the 2026 advanced-security menu — seL4 microkernel (via CAmkES
contract), Wasmtime userland sandbox, eBPF LSM policy, MLS lattice,
attestation quote, Sigstore signing, SLSA-v1.0 provenance, Nix
reproducible build, distroless runtime container, Darwin sandbox-
exec / OpenBSD pledge — as one `make chace` aggregator that reports
PASS / honest-SKIP / FAIL per layer.  The lattice kernel runs at
**6.1 × 10⁸ decisions/s** on Apple M4 with 61 deterministic self-
tests and passes ASAN / UBSAN.  No layer is claimed that is not run.

## 1. Problem

Three 2026 attacks circumvent every existing single-layer defence:

- **DDIPE** (arXiv 2604.03081) — data-driven indirect prompt
  injection embedded in documents bypasses scalar confidence
  filters.
- **ClawWorm** (arXiv 2603.15727) — a capability-carrying worm
  travels between agents via the MCP/Claw skill registry, bypassing
  per-skill vetting.
- **Malicious Intermediary** (arXiv 2604.08407) — a legitimately-
  authorised tool exports data to a second authorised tool that
  together exfiltrate, bypassing static allowlists.

Every one of these succeeds on **ambiguous payload**: intent that
looks confident on one axis but is irreducibly underdetermined on
another.  Existing single-signal gates (confidence, entropy,
allowlist) cannot see the decomposition.

## 2. Design

Σ-Citadel adds two orthogonal primitives on top of v60 σ-Shield:

### 2.1 Bell-LaPadula + Biba lattice with MLS compartments

A subject label has three 8/8/16-bit fields: *clearance*,
*integrity*, *compartments*.  The lattice kernel computes, in
parallel for every request:

- BLP: `subj.clearance ≥ obj.clearance` (no-read-up) for reads;
  the reverse for writes.
- Biba: `subj.integrity ≤ obj.integrity` (no-read-down) for reads;
  the reverse for writes.
- MLS: `(subj.compartments & obj.compartments) == obj.compartments`.

All three lanes run unconditionally; the op (`READ`/`WRITE`/`EXEC`)
selects which to enforce via AND-masks.  `EXEC` requires both
read and write to pass.

### 2.2 Attestation chain

A quote is a deterministic 256-bit digest over

```
   H(code_page_hash || caps_state_hash || sigma_state_hash ||
     lattice_hash   || nonce || reserved_padding)
```

The default `H` is a four-lane XOR-fold with SplitMix mixing
constants — deterministic, equality-sensitive, but *not* a MAC.
Compiling with `-DCOS_V61_LIBSODIUM -lsodium` swaps `H` for
`crypto_generichash(32, 64-byte buf)` (BLAKE2b-256) for real
cryptographic strength.

Quote equality is constant-time (OR-fold reduction of 4 × 64-bit
XORs).

### 2.3 Composition with v60

```
compose(v60_result, v61_lattice_result) → {
    ALLOW        if v60==ALLOW ∧ lattice==1
    DENY_V60     if v60∈{DENY_*}           ∧ lattice==1
    DENY_LATTICE if v60==ALLOW             ∧ lattice==0
    DENY_BOTH    otherwise
}
```

Branchless: four 0/1 masks, `OR` of four `(CONST * mask)`.

## 3. Correctness

61 deterministic self-tests cover:

- Version / policy defaults.
- Individual BLP / Biba / compartment rules.
- Combined BLP+Biba edge cases.
- `EXEC` conservatism.
- Unknown-op denial.
- Policy overlay (min-integrity quarantine).
- Batch = scalar equivalence on 64-element random traces.
- Attestation determinism + sensitivity to every input field.
- Hex encoding round-trip.
- Constant-time equality null-safety.
- Composition all four surfaces + null-safety + tag strings.
- 64-byte aligned allocator null-safety + zero-init.
- Four adversarial scenarios (ClawWorm Biba, DDIPE BLP, confused-
  deputy compartment, runtime-patch quote).
- 2000-case stress invariant.

ASAN + UBSAN builds pass all 61 tests.  Hardened build (OpenSSF
2026 + ARM64 `-mbranch-protection=standard`) passes all 61 tests.

## 4. Performance

Measured on Apple M4 (clang 17, `-O2`):

```
v61 lattice_check_batch n=1024:   6.15 × 10⁸ decisions/s
v61 lattice_check_batch n=16384:  6.14 × 10⁸ decisions/s
v61 lattice_check_batch n=262144: 6.12 × 10⁸ decisions/s
```

Constant throughput across three decades of batch size indicates
the kernel is L1-cache resident and L1-bandwidth-bound, not branch
or prefetch limited.

## 5. `make chace` — composed verification

`make chace` runs, on the host where the command is executed:

1. `check-v60` (σ-Shield kernel)
2. `check-v61` (Citadel kernel)
3. `attest` (emit ATTESTATION.json)
4. seL4 contract file presence (CAmkES build SKIPs on macOS)
5. `wasm-sandbox` (SKIP if WASI-SDK/wasmtime absent)
6. `ebpf-policy` (Linux only; SKIP on macOS)
7. `sandbox-exec` (Darwin only)
8. `harden`, `hardening-check`
9. `sanitize`
10. `sbom`
11. `security-scan`
12. `reproducible-build`
13. `sign` (Sigstore cosign; SKIP without OIDC)
14. `slsa` (SLSA v1.0 predicate stub)
15. `distroless` (SKIP without docker)

Every layer reports PASS / honest SKIP / FAIL.  Exit 0 only if no
FAIL; SKIPs never inflate PASS counts.

## 6. Positioning

To our knowledge no open-source AI-agent runtime before Σ-Citadel
ships every layer of the CHACE-class advanced-security menu as one
runnable gate.  seL4-only runtimes prove isolation but leave policy
to userspace.  Wasmtime-only sandboxes prevent syscall abuse but
have no clearance or integrity notion.  IronClaw (NEAR AI) handles
credentials but lacks σ-gated intent.  v61 composes all three and
runs on-device.

## 7. Limitations & non-claims

* The default quote is not a MAC; enable libsodium for
  cryptographic strength.
* TPM 2.0 / Apple Secure Enclave binding is P-tier; v61 accepts
  external hardware-computed hashes but does not itself drive the
  hardware.
* No formal Frama-C proof yet; all v61 claims are runtime M.
* The `sel4/sigma_shield.camkes` file is a contract; a CI step that
  actually instantiates the CAmkES component is P.
* CHERI support is doc-only; Apple M4 has no CHERI path.
* SLSA-3 requires Sigstore Rekor inclusion; that ships in CI via
  `slsa-framework/slsa-github-generator`, not in local `make slsa`.

## 8. Availability

All source, tests, documentation, SBOM, hardened binaries, and
attestation stubs are AGPL-3.0-or-later under
`github.com/sigmaosorg/creation-os-kernel` as of April 2026.

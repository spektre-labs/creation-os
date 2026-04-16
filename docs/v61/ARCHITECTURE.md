# Creation OS v61 — Σ-Citadel architecture

## Wire map

```
  request
     │
     ▼
  ┌───────────────────────────────────────┐
  │ v60 σ-Shield  (authorise, 5-valued)   │     ←── code-page integrity,
  │   ALLOW / DENY_CAP / DENY_INTENT /    │         sticky-deny,
  │   DENY_TOCTOU / DENY_INTEGRITY        │         TOCTOU arg-hash,
  └───────────────────────────────────────┘         σ=(ε,α) intent gate
                    │
                    ▼   (only if v60 returns ALLOW does the request
                         reach the lattice layer at all)
  ┌───────────────────────────────────────┐
  │ v61 Σ-Citadel  (lattice check)        │     ←── BLP clearance,
  │   ALLOW / DENY_LATTICE                │         Biba integrity,
  │                                       │         MLS compartments,
  │                                       │         min-integrity policy
  └───────────────────────────────────────┘
                    │
                    ▼
     compose:  {ALLOW, DENY_V60, DENY_LATTICE, DENY_BOTH}
                    │
                    ▼
  ┌───────────────────────────────────────┐
  │ tool execution  (Wasmtime / seL4)     │     ←── ambient authority
  │   (outside this process via cap ring) │         disabled; only
  │                                       │         paths σ-Shield
  │                                       │         authorised are bound
  └───────────────────────────────────────┘
                    │
                    ▼
  ┌───────────────────────────────────────┐
  │ attestation quote                     │     ←── H(code || caps ||
  │   (BLAKE2b-256 via libsodium opt-in;  │            σ-state || lattice
  │    XOR-fold-256 deterministic fallback)│           || nonce)
  └───────────────────────────────────────┘
                    │
                    ▼
  ┌───────────────────────────────────────┐
  │ offline verifier  (Sigstore / cosign) │     ←── keyless OIDC in CI;
  │   baseline comparison + Rekor         │         SKIP in local dev
  └───────────────────────────────────────┘
```

## Per-surface tier claims

| Surface                          | Tier | Notes                                   |
|----------------------------------|------|-----------------------------------------|
| Lattice check core               | M    | 61 deterministic tests; branchless       |
| Lattice policy overlay           | M    | min-integrity quarantine rule            |
| Batch lattice check              | M    | prefetch 16 lanes ahead                  |
| Attestation quote (XOR-fold)     | M    | deterministic, equality-sensitive        |
| Attestation quote (BLAKE2b)      | M (when `COS_V61_LIBSODIUM=1`) | libsodium crypto_generichash |
| CT equality                      | M    | constant-time OR-fold reduction          |
| Composition with v60             | M    | 4-way branchless mux                     |
| seL4 CAmkES component            | I    | `sel4/sigma_shield.camkes` contract only |
| Wasmtime sandbox                 | I    | `wasm/example_tool.c` + harness          |
| eBPF LSM policy                  | I    | Linux only; SKIP on macOS                |
| Darwin sandbox-exec              | I    | `sandbox/darwin.sb`                      |
| OpenBSD pledge/unveil            | I    | `sandbox/openbsd_pledge.c`               |
| Nix reproducible build           | I    | `nix/v61.nix`                            |
| Distroless container             | I    | `Dockerfile.distroless`                  |
| Sigstore sign (cosign)           | P    | keyless OIDC in CI only                  |
| SLSA-3 provenance                | P    | `.github/workflows/slsa.yml`             |
| TPM 2.0 / Secure Enclave binding | P    | v61 accepts external hash input          |
| CHERI capability pointers        | P    | hardware unavailable                      |

## Hardware discipline

Per `.cursorrules`:

1. `aligned_alloc(64, …)` for every allocation.
2. No dynamic allocation on lattice / quote hot paths.
3. Branchless lattice compare (AND-mask cascade).
4. Constant-time 256-bit quote equality (OR-fold reduction).
5. Prefetch 16 lanes ahead in batch lattice check.
6. No syscalls on hot paths; attestation is pure compute.

## Measured performance (Apple M4, clang -O2)

```
$ make microbench-v61
v61 lattice_check_batch n=1024:   6.15e+08 decisions/s
v61 lattice_check_batch n=16384:  6.14e+08 decisions/s
v61 lattice_check_batch n=262144: 6.12e+08 decisions/s
```

Five-to-six-fold higher throughput than v60 σ-Shield (which is already
1.7-1.8 × 10⁸ decisions/s) because the lattice kernel computes fewer
fields and has no hash lanes.  In practice the composition gate
(`cos_v61_compose(v60_result, v61_result)`) is bounded by v60, not by
v61.

## Threat model tie-in (see `THREAT_MODEL.md`)

* **ClawWorm** (arXiv 2603.15727) — low-integrity worm payload tries
  to write high-integrity code page → Biba no-write-up stops it
  even if σ-Shield somehow authorised.
* **DDIPE** (arXiv 2604.03081) — low-clearance doc-reader tries to
  read high-clearance secrets → BLP no-read-up stops it even if σ
  mistakes intent as confident.
* **Malicious Intermediary** (arXiv 2604.08407) — HR tool tries to
  access billing compartment → compartment mask rejects.
* **Runtime patch** — attacker flips bit in code page → next quote
  mismatches baseline → offline verifier halts the system.

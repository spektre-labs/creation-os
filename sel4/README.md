# seL4 + CAmkES integration for σ-Shield + Σ-Citadel

`sigma_shield.camkes` is a **design contract** for running Creation OS
v60 σ-Shield + v61 Σ-Citadel as a formally-isolated component under
the seL4 microkernel.

## Tier claim

- **I** (interpreted, documented): the contract itself.
- **P** (planned): a CI target that builds the CAmkES component on the
  `sel4test` harness.  Not yet wired.

## Why seL4

seL4 is the only microkernel with a full machine-checked proof
(functional correctness, integrity, and confidentiality) for its
core ABI.  Running σ-Shield as an seL4 component lifts v60's M-tier
runtime authorise into an **F-tier-by-composition** claim: *given* the
seL4 proof, capability isolation between the Shield and the Tool
Sandbox is mathematical, not empirical.

## What this file encodes

1. **Three** outward-facing endpoints: `authorize`, `lattice_check`,
   `attest_quote`.
2. **Zero** network or filesystem capabilities on the Shield component.
3. **Bounded** request/response rings between Shield and Tool.
4. Offline verifier as a sibling component — never on the tool path.

## Running it (when you have the seL4 SDK)

```
$ camkes-build sigma_shield.camkes
$ ninja -C build
$ ./simulate
```

On hosts without the SDK, `make chace` reports
`sel4: SKIP (camkes not on PATH)` and continues — never silent PASS.

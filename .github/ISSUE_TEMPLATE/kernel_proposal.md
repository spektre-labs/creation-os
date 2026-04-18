---
name: Kernel proposal — full RFC
about: A full design proposal for a new vNNN kernel (used by maintainers).
title: "[rfc] vNNN σ-<name>: <one-line summary>"
labels: ["rfc", "kernel-proposal", "needs-triage"]
assignees: []
---

<!--
This template is the long form of `feature_request.md`. Use it when
the kernel:
  * crosses more than one existing layer,
  * introduces a new σ channel or aggregator,
  * exposes a new public HTTP route,
  * touches licensing, commercial terms, or governance,
  * changes the merge-gate threshold of an existing gate.

Structural changes require BDFL sign-off (see GOVERNANCE.md).
-->

## 1. Abstract

<!-- 2–3 sentences. -->

## 2. Motivation

<!-- What fails today without this kernel? Cite the existing kernel(s)
that are inadequate. Link to any archived numbers or external papers. -->

## 3. σ-contract

Exact formula for σ_<name>, the τ threshold, and the enforcement behavior.

## 4. API surface

```
/src/vNNN/<name>.h   <- public C API
/src/vNNN/<name>.c   <- implementation
/src/vNNN/main.c     <- CLI (self-test, demo subcommands)
```

HTTP surface (if any):

```
GET  /v1/<path>
POST /v1/<path>
```

## 5. Interactions

Which kernels does this one invoke, and which ones invoke it?
Draw the σ-flow arrows.

## 6. Merge-gate

All falsifiable checks, with expected stdout fragments.

## 7. vNNN.0 vs vNNN.1 split

## 8. Alternatives considered

## 9. Risks + mitigations

## 10. Checklist

- [ ] The contract has a concrete falsifiable test.
- [ ] No v0 claim requires network, weights, or external tools.
- [ ] v1 split is named and not tangled with v0.
- [ ] I have run `make merge-gate` on a scratch branch with the
      new `check-vNNN` target wired in and it passes.

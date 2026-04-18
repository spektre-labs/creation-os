---
name: Feature request — new kernel or capability
about: Propose a new σ-governed kernel for Creation OS.
title: "[feature] vNNN σ-<name>: <one-line summary>"
labels: ["feature", "kernel-proposal", "needs-triage"]
assignees: []
---

<!--
Before opening a feature request, please read:
  - docs/CLAIM_DISCIPLINE.md
  - docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md
  - GOVERNANCE.md (merge requirements + the 1 = 1 invariant)

Creation OS is kernel-per-capability. A feature request that is
not framed as a new vNNN kernel is almost always a smell. If you
think you have one, say so — the maintainer may redirect it as a
patch to an existing kernel.
-->

## Problem (what is missing today)

<!-- One paragraph. What does Creation OS fail to do / measure /
prove today? Be specific. -->

## Proposed kernel

**Kernel id.**  vNNN
**Kernel name.** σ-<name>
**Layer.**      <silicon | generation | σ-governance | reasoning +
                 agentic | training + persistence | distribution +
                 collective | metacognition>

## σ-contract

What σ does this kernel compute, and what invariant does it enforce?

```
σ_name = <formula>
contract: σ_name <= τ_name  ⇒  <behavior>
          σ_name >  τ_name  ⇒  <abstain / halt / pause>
```

## Merge-gate

Propose the merge-gate target name and the ≥ 4 falsifiable checks
it will run.

```
make check-vNNN-<slug>
  * self-test exits 0
  * <contract 1>
  * <contract 2>
  * determinism (same seed → byte-identical output)
```

## v0 vs v1 split

* **vNNN.0** — deterministic, weights-free, framework-free, offline
  implementation. No network, no external tools beyond libm.
* **vNNN.1** — the live-hardware / live-corpus / live-network
  follow-up.

## Honest claims

<!-- What is this kernel explicitly NOT doing? What measurements
are tier-0 synthetic vs tier-1 archived vs tier-2 live? -->

## Checklist

- [ ] I have read `docs/CLAIM_DISCIPLINE.md` and
      `GOVERNANCE.md#merge-requirements`.
- [ ] The kernel follows the `src/vN/` + `benchmarks/vN/` +
      `docs/vN/` layout.
- [ ] I have signed the `CLA.md` (or will before the first PR).
- [ ] I am not silently deleting any existing **Limitations** note
      in the README.

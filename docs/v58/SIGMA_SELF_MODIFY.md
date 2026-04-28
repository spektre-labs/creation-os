# v58 — σ-governed self-modification (lab scaffold)

<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
-->

## Code

- **`python/cos/sigma_self_modify.py`** — `SigmaSelfModifier`: preflight each mutation with `sigma_gate_core` before calling your `run_mutation` callback; invariant substrings (including `sigma_gate.h`, `sigma_sampler.h`, `LICENSE`, SPDX, `AGENTS.md`, `codex`) force **ABSTAIN** → mutation is **not** executed.

## What this is / is not

- **Is:** a **policy scaffold** to cut wasted runs on obviously forbidden or oversized diffs, using the same verdict machine as runtime σ.  
- **Is not:** a Darwin Gödel Machine replacement, formal safety proof, or automatic SWE-bench winner. Empirical scores still come from **your** `run_mutation` and benchmark harness.

## Invariants (operator rule)

Do not edit **`python/cos/sigma_gate.h`** (or the sampler header) in self-mod flows; the scaffold rejects mutations whose **text** references those paths. Real enforcement still requires review + CI + restricted filesystem permissions.

## Next (v59–v60)

- **v59** — living weights / TTT hooks (separate module + eval).  
- **v60** — recursive loop orchestration + end-to-end eval.

See [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) before publishing any improvement percentages.

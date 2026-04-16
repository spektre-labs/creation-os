# The Verified Agent: Tier-Honest Composition of Sandboxing, σ-Governed Reasoning, and Multi-LLM σ-Triangulation

**Spektre Labs · Helsinki · ORCID 0009-0006-0903-8541**
**Creation OS v57 · AGPL-3.0**

> *Status: position paper draft.  Not peer-reviewed.  Reproduces with*
> `make verify-agent` *on any UNIX host with a C11 compiler; missing*
> *external verification tools degrade to honest* `SKIP` *lines.*

## Abstract

Open-source agent frameworks of 2025–2026 ship empirical
sandboxing — Docker containers, WebAssembly isolation, capability
allowlists — and rely on prompt-engineered reasoning loops on
top.  None of them can answer the basic question: *what is the
agent provably unable to do?*  Their user-facing answer is
"trust our architecture."  We propose a different answer:
**run** `make verify-agent`.  We describe v57, the convergence
artifact of the Creation OS family (v33–v56), which composes a
formally-annotated σ-kernel surface (Frama-C / WP), a capability
sandbox with a 7-layer defense (v48), a σ-governed harness loop
(v53), a σ-triangulating multi-LLM proconductor (v54), a
σ-decomposed speculative-decoding policy (v55), and a
σ-Constitutional self-modification controller with a rule-based
process verifier and a σ-gated in-place test-time-training budget
(v56).  v57 introduces no new mathematics.  Its sole contribution
is an *invariant + composition registry* whose every claim is
explicitly tagged with one of four tiers — runtime-checked (M),
formally proven (F), interpreted / documented (I), or planned (P)
— and a single shell driver that aggregates the existing make
targets into one report that distinguishes `PASS`, `SKIP`, and
`FAIL` per slot.

## 1. Motivation

A 2026 wave of open-source Claude-Code-style agents demonstrated
the structural problem in the field: the reasoning layer cannot
be secured, so the execution layer must be sandboxed.  This
insight is correct but incomplete.  When the sandbox is the only
defense, every reasoning failure becomes a sandbox-execution
event.  When the multi-LLM router optimises for cost, every
disagreement between models becomes a confident wrong answer.
When the model edits itself between tokens (in-place test-time
training, IP-TTT), every weight update bypasses every prior
check.

These failures are *compositional*.  No single layer fixes them.
A complete agent runtime must compose:

1. A **bounded execution layer** (sandbox + capability set).
2. A **σ-governed reasoning layer** that refuses to emit when
   epistemic uncertainty exceeds the per-task threshold.
3. A **multi-LLM strategy** that abstains on disagreement
   rather than picking a side by majority vote.
4. A **constitutional self-modification controller** that gates
   IP-TTT updates by σ drift and rolls back on σ regression.
5. A **deterministic process verifier** as the false-positive
   floor under any learned PRM.

Creation OS already shipped each of these as a separate version
(v48, v53, v54, v56).  v57's contribution is the table that names
the composition and the command that verifies it.

## 2. Design

### 2.1 Slot table

The Verified Agent declares nine **composition slots**.  Each
slot is owned by one or more existing Creation OS versions.
Each slot has exactly one **make target** that runs the
deterministic check, the formal proof, or the assurance step
backing the slot's claim.  Each slot has one **best tier**:

```
M = runtime-checked deterministic self-test
F = formally proven proof artifact
I = interpreted / documented
P = planned (next step explicit)
```

The table is the single source of truth.  A static report is
emitted by `creation_os_v57 --architecture` and
`creation_os_v57 --verify-status`.

### 2.2 Invariant table

The Verified Agent declares five **invariants**.  Each carries
up to four checks (runtime, formal, documented, planned).  Each
check carries an enabled flag, a tier, a target string, and a
description.  The five invariants chosen for v57 span the surface
that ad-hoc agent frameworks typically *trust the architecture*
on:

1. `sigma_gated_actions`
2. `bounded_side_effects`
3. `no_unbounded_self_modification`
4. `deterministic_verifier_floor`
5. `ensemble_disagreement_abstains`

The full statements appear in `src/v57/invariants.c`.

### 2.3 Self-test discipline

`creation_os_v57 --self-test` runs 49 deterministic tests on the
registry itself.  These tests do not claim the invariants hold;
they claim the registry is well-formed: counts, uniqueness, tier
monotonicity, presence of mandatory fields, find-by-id, histogram
sums.  This is a falsifiability harness for the *meta-claims*
v57 makes about how it talks about itself.

### 2.4 Live aggregate

`scripts/v57/verify_agent.sh` walks the slot table, dispatches
each slot's `make` target, and reports per-slot `PASS`, `SKIP`,
or `FAIL`.  A `SKIP` is emitted when the make target ran without
error but matched the regex `SKIP\b` in its output (the existing
Creation OS verification pipeline emits this when external tools
like Frama-C, sby, pytest, garak are not installed).  A `FAIL`
is emitted on non-zero exit code.  `--strict` treats `SKIP` as
`FAIL`; `--json` writes a machine-readable report; `--quiet`
emits only the final summary line.

The aggregate **never silently downgrades**.  Missing tooling is
never a `PASS`.

## 3. What v57 does not introduce

- **No new σ math.** Every σ channel and every σ policy v57
  uses already shipped in v33–v56.
- **No new C runtime.** v57 is two `.c` files, two `.h` files,
  one driver, and one shell script.
- **No model.** v57 has no opinion about which LLM is in the
  `multi_llm_routing` slot.
- **No socket.** Invariant #3 of `creation.md` applies: the
  composition registry must not open the network.
- **No allocation on a hot path.** The registry is `static const`.

## 4. What v57 deliberately does not claim

A position paper is dishonest without an explicit non-claims
list.  v57 does not claim:

1. **FAA / EASA certification.** v49's DO-178C-aligned artifacts
   are tier I.  They are *assurance documents*, not a
   certificate.
2. **Zero CVEs.** The surface is small by construction (no
   network, no plugin loader, no IPC beyond stdin/stdout in
   most labs), but smallness is not absence.
3. **Frontier-model accuracy.** The agent runtime composes
   around whichever model is supplied by the routing slot.
   v57 has nothing to say about that model's MMLU.
4. **A replacement for any specific commercial product.**

## 5. Reproducibility

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make standalone-v57
./creation_os_v57 --self-test          # 49/49 PASS, deterministic
./creation_os_v57 --architecture       # static slot + invariant table
./creation_os_v57 --verify-status      # static tier histogram
make verify-agent                      # live aggregate; reports honest SKIP/PASS/FAIL
```

For CI environments that ship every external tool:

```bash
bash scripts/v57/verify_agent.sh --strict --json reports/v57.json
```

## 6. Discussion

The structural defect of "trust our architecture" is that it
makes the architecture's claims unfalsifiable from the outside.
Once a framework's safety is a property of its design rather
than of its execution, regressions silently downgrade — there is
no test that fails when the design no longer matches reality.

v57's tier table is a small but real change: every claim is
attached to a make target, every make target either passes,
skips honestly, or fails honestly, and every promotion to a
higher tier requires shipping a proof or a deterministic test
rather than narrative.  This makes the composition's claims
**externally falsifiable**.

That is not the same as making the agent *safe*.  It is the
weaker, more honest claim that the agent's safety is *checkable*
— by us, by reviewers, by anyone with a UNIX host and a copy of
the repo.

## 7. Outlook

The next concrete promotions, in priority order:

1. `bounded_side_effects` from M to F via a TLA+ specification
   of the sandbox-policy subset relation
   (`proofs/v57/sandbox.tla`).
2. `sigma_gated_actions` from F-on-kernel-surface to F-on-
   harness-surface via expanded ACSL contracts on v53.
3. `do178c_assurance_pack` slot from I to M via `make certify`
   green on a CI host with all required tooling installed.

Each promotion updates one row in `src/v57/invariants.c` or
`src/v57/verified_agent.c` and is independently verifiable.

## Acknowledgements

The driving oivallus for v57 came from a working session on the
state of open-source agent frameworks in Q2 2026.  Specific
project names referenced in that session are stylized
(OpenClaw / IronClaw / ZeroClaw / Claude Code) and the CVE /
ARR figures cited there are not independently audited in this
paper.

## See also

- `docs/v57/THE_VERIFIED_AGENT.md` — the artifact in one page
- `docs/v57/ARCHITECTURE.md` — composition map and tier semantics
- `docs/v57/POSITIONING.md` — vs ad-hoc agent sandboxes
- `docs/WHAT_IS_REAL.md` — repo-wide tier table
- `creation.md` — project invariants

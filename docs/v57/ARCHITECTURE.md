# v57 Architecture — Verified Agent Convergence

## One sentence

v57 is the **invariant + composition registry** that turns
Creation OS v33–v56 from a corpus into a single named product
whose claims are explicitly tiered and aggregately verifiable
with one command.

## Wire map

```
                ┌─────────────────────────────────────────────────────────┐
                │                v57 Verified Agent                       │
                │                                                         │
                │  invariants[]  ──── 5 entries, tiered M / F / I / P    │
                │  slots[]       ──── 9 entries, owned by v47..v56,v49   │
                │  report_build  ──── histogram per tier                  │
                │                                                         │
                │  driver:  creation_os_v57 --self-test  (49/49 PASS)     │
                │  driver:  creation_os_v57 --architecture                │
                │  driver:  creation_os_v57 --positioning                 │
                │  driver:  creation_os_v57 --verify-status               │
                │                                                         │
                │  live aggregate:  scripts/v57/verify_agent.sh           │
                │     ──► dispatches each slot's owning make target       │
                │     ──► PASS / SKIP / FAIL per slot, never silent       │
                │     ──► --strict treats SKIP as FAIL                    │
                │     ──► --json emits machine-readable report            │
                └─────────────────────────────────────────────────────────┘
                              │
   ┌──────────┬───────────────┼───────────────┬──────────┬───────────┐
   ▼          ▼               ▼               ▼          ▼           ▼
┌─────┐    ┌─────┐         ┌─────┐         ┌─────┐    ┌─────┐    ┌─────┐
│ v48 │    │ v47 │         │ v53 │         │ v54 │    │ v55 │    │ v56 │
│sand-│    │ACSL │         │ σ-  │         │ σ-  │    │ σ₃- │    │ σ-  │
│box  │    │σ-   │         │TAOR │         │proc │    │spec │    │const│
│+ DL │    │krnl │         │loop │         │ond. │    │     │    │     │
└─────┘    └─────┘         └─────┘         └─────┘    └─────┘    └─────┘
   M          F               M               M          M          M

                       ┌─────┐         ┌─────┐
                       │ v49 │         │v48+ │
                       │DO-  │         │v49  │
                       │178C │         │red  │
                       │pack │         │team │
                       └─────┘         └─────┘
                          I              I
```

## Tier semantics (no blending)

| tier | meaning                                         | example                          |
|------|-------------------------------------------------|----------------------------------|
| **M** | runtime-checked deterministic self-test         | `make check-v53` returns 0       |
| **F** | formally proven proof artifact                  | `src/v47/sigma_kernel_verified.c` ACSL contracts (Frama-C/WP) |
| **I** | interpreted / documented (no mechanical check)  | `docs/v49/certification/` HLR/LLR/PSAC/SCMP/SDD/SDP/SQAP/SVP |
| **P** | planned next step                               | TLA+ specification of the v48 sandbox-policy subset |

A check is **enabled** iff its corresponding `target` (or, for
P-tier entries, its `description`) is non-NULL.  An invariant's
**best tier** is the strongest enabled tier across its four
checks (M > F > I > P).  The `creation_os_v57 --self-test`
driver verifies these properties on every build.

## What each slot owns

### `execution_sandbox` (v48, M)
The deterministic capability sandbox + 7-layer defense.
`make check-v48` runs the sandbox + `defense_layers` self-tests.
Owns the **bounded_side_effects** invariant.

### `sigma_kernel_surface` (v47, F)
Frama-C / WP ACSL contracts on the σ-kernel C surface.
`make verify-c` discharges the contracts when Frama-C is
installed; otherwise it `SKIP`s honestly.  Provides the F-tier
backing for the **sigma_gated_actions** invariant.

### `harness_loop` (v53, M)
σ-TAOR loop with σ-abstention, `creation.md`-style invariants,
σ-triggered subagent dispatch, σ-prioritized context
compression.  `make check-v53` runs the 13 deterministic
self-tests.  Owns the runtime side of **sigma_gated_actions**.

### `multi_llm_routing` (v54, M)
σ-proconductor: σ-profiled subagents, σ-triangulated
convergence, abstention on disagreement, profile learning.
`make check-v54` runs the v54 self-tests.  Owns the
**ensemble_disagreement_abstains** invariant.

### `speculative_decode` (v55, M)
σ₃-speculative scaffold: per-channel σ decomposition (entropy /
top-margin / agreement) + EARS commit / EASD speculative-depth
control.  `make check-v55` runs the v55 self-tests.

### `constitutional_self_modification` (v56, M)
σ-Constitutional: VPRM-style rule verifier + σ-gated IP-TTT
budget controller + grokking commutator-σ channel + ANE
matmul→1×1 conv layout helper.  `make check-v56` runs the
56-test self-test.  Owns the
**no_unbounded_self_modification** and
**deterministic_verifier_floor** invariants.

### `do178c_assurance_pack` (v49, I)
DO-178C DAL-A-aligned assurance documents under
`docs/v49/certification/`: ASSURANCE_HIERARCHY, HLR, LLR, PSAC,
SCMP, SDD, SDP, SQAP, SVP.  Tier I — these are *artifacts*,
not a certificate from FAA / EASA.  `make certify` runs the
companion driver and `SKIP`s honestly when expected tooling is
absent.

### `red_team_suite` (v48 + v49, I)
Garak + DeepTeam + σ-property red-team aggregator.
`make red-team` dispatches the suite and `SKIP`s honestly
when the external red-team tooling is not installed.

### `convergence_self_test` (v57, M)
The v57 registry self-test itself.  `make check-v57` runs the
49 deterministic registry tests (count, uniqueness, tier
monotonicity, find-by-id, histogram sums, slot completeness).

## What v57 does not add

- No new σ math
- No new C runtime
- No socket
- No memory allocation on a hot path (registry is `static const`)
- No plugin loader
- No model weights

The footprint is intentionally small: two source files
(`invariants.c`, `verified_agent.c`), one driver
(`creation_os_v57.c`), one shell script (`verify_agent.sh`),
and four documents.  Smaller surface = smaller attack surface
= cheaper to verify.

## How v57 composes with the rest

v57 reads the existing make targets; it does not modify them.
Removing v57 leaves the rest of the repo unchanged.  Adding a
new slot means:

1. Append a row to `k_slots[]` in `src/v57/verified_agent.c`
   with an honest tier tag.
2. Append the corresponding entry to `SLOTS=(...)` in
   `scripts/v57/verify_agent.sh`.
3. Update `docs/v57/THE_VERIFIED_AGENT.md` and the relevant
   row of `docs/WHAT_IS_REAL.md`.
4. Run `make check-v57` (registry self-test will validate the
   new row's basic invariants — id present, target string
   non-NULL, etc.).

## Invariants tracked today

See `src/v57/invariants.c` for the authoritative table.  Five
invariants:

1. **`sigma_gated_actions`** — every emitted action is σ-gated.
2. **`bounded_side_effects`** — every dispatched tool fits the
   active sandbox capability set.
3. **`no_unbounded_self_modification`** — every IP-TTT update
   is σ-gated, byte-budgeted, and rolled back if σ regresses.
4. **`deterministic_verifier_floor`** — every reasoning chain
   is graded by a deterministic rule verifier; verifier-σ is
   the false-positive floor under any learned PRM.
5. **`ensemble_disagreement_abstains`** — multi-LLM ensembles
   abstain on disagreement instead of picking a side.

Each invariant carries up to four checks (runtime, formal,
documented, planned).  All five reach **M** today via at least
one runtime check; one (`sigma_gated_actions`) also reaches
**F** via the v47 ACSL surface (subject to Frama-C availability).

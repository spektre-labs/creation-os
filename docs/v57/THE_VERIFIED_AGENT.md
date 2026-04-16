# The Verified Agent — Creation OS v57

## What this is

One artifact. AGPL-3.0. Runs locally. C11 + tiny shell driver.
Composes the Creation OS subsystems v33–v56 into one named product
with one verification command.

## What it is not

- A new σ channel
- A new C runtime
- A new sandboxing approach
- A new Rust rewrite
- A skill registry with vetting promises
- An accuracy claim against frontier models

v57 introduces **zero new mathematics**.  Every piece of math the
Verified Agent relies on already shipped in v33–v56.  v57 names the
composition and forces every claim into one of four explicit tiers.

## Why it is different

The driving oivallus of Q2 2026 is that the open-source agent
framework field offers ad-hoc sandboxing — Docker containers, WASM
sandboxes, "best practices" — and every framework's user-facing
answer is the same:

> Trust our architecture.

Creation OS v57's user-facing answer is different:

> Run `make verify-agent`.

The script walks the composition slots declared in
`src/v57/verified_agent.c`, dispatches each slot's owning make
target, and reports — per slot — `PASS`, `SKIP`, or `FAIL`.

It never silently downgrades.  A missing tool (Frama-C, sby,
pytest, garak, …) is a `SKIP`, not a `PASS`.  The exit code is
non-zero if anything `FAIL`s.

## What is being claimed (with tiers)

The Verified Agent declares **nine composition slots** and
**five invariants**.  Each row carries an honest tier tag.  Tiers
are explicit, not blended:

- **M** — runtime-checked by a deterministic self-test that
  returns 0 iff the check passes
- **F** — formally proven; a proof artifact (Frama-C / TLA+ / sby /
  Coq) discharges the claim
- **I** — interpreted / documented; the claim is reasoned about
  in prose with pointers to the relevant code, but not yet checked
  mechanically
- **P** — planned; the next concrete step that would elevate the
  claim to M or F

### Composition slots (today)

| tier | slot                                | owner          | make target           |
|------|-------------------------------------|----------------|-----------------------|
| M    | `execution_sandbox`                 | v48            | `make check-v48`      |
| F    | `sigma_kernel_surface`              | v47            | `make verify-c`       |
| M    | `harness_loop`                      | v53            | `make check-v53`      |
| M    | `multi_llm_routing`                 | v54            | `make check-v54`      |
| M    | `speculative_decode`                | v55            | `make check-v55`      |
| M    | `constitutional_self_modification`  | v56            | `make check-v56`      |
| I    | `do178c_assurance_pack`             | v49            | `make certify`        |
| I    | `red_team_suite`                    | v48 + v49      | `make red-team`       |
| M    | `convergence_self_test`             | v57            | `make check-v57`      |

### Invariants (today)

| tier | invariant                              | best owning version |
|------|----------------------------------------|---------------------|
| M    | `sigma_gated_actions`                  | v53 + v34 (with v47 F-tier surface) |
| M    | `bounded_side_effects`                 | v48                 |
| M    | `no_unbounded_self_modification`       | v56                 |
| M    | `deterministic_verifier_floor`         | v56                 |
| M    | `ensemble_disagreement_abstains`       | v54                 |

The static report is also available at any time:

```
make standalone-v57
./creation_os_v57 --architecture
./creation_os_v57 --verify-status
./creation_os_v57 --positioning
```

## What `make verify-agent` actually does

```
$ make verify-agent
  [M] execution_sandbox                    (make check-v48     )  ... PASS
  [F] sigma_kernel_surface                 (make verify-c      )  ... SKIP
  [M] harness_loop                         (make check-v53     )  ... PASS
  [M] multi_llm_routing                    (make check-v54     )  ... PASS
  [M] speculative_decode                   (make check-v55     )  ... PASS
  [M] constitutional_self_modification     (make check-v56     )  ... PASS
  [I] do178c_assurance_pack                (make certify       )  ... SKIP
  [I] red_team_suite                       (make red-team      )  ... SKIP
  [M] convergence_self_test                (make check-v57     )  ... PASS

  PASS: 6 / 9
  SKIP: 3 / 9
  FAIL: 0 / 9
```

The `SKIP` lines are honest reports that the backing tooling
(Frama-C / WP, Garak, DeepTeam, certain pytest extras) is not
installed on this machine.  Add `--strict` to treat `SKIP` as
`FAIL` for CI environments where every tool is expected to be
present:

```
bash scripts/v57/verify_agent.sh --strict
```

## What v57 does **not** claim

This file would be dishonest without an explicit list of
non-claims.  v57 does not claim:

1. **FAA / EASA certification.**  v49's DO-178C-aligned artifacts
   are in tier I.  They are *assurance documents*, not a
   certificate.
2. **Zero CVEs.**  Creation OS has no public CVE history because
   it is a small AGPL-licensed research kernel, not because the
   surface is mathematically empty.  The surface is, by
   construction, small (no network, no IPC beyond stdin/stdout in
   most labs, no plugin loader), but smallness is not absence.
3. **Frontier-model accuracy.**  The agent runtime composes around
   whichever local or remote model is supplied by the
   `multi_llm_routing` slot.  v57 has nothing to say about that
   model's MMLU.
4. **A replacement for any specific commercial product.**  The
   `--positioning` banner uses category names (`ad-hoc OSS agent`,
   `enterprise SaaS`) drawn from the driving oivallus prompt; CVE
   counts and ARR figures cited there are user-supplied reporting,
   not independent audit.

## How to compose with the rest of Creation OS

v57 sits **above** v33–v56 and **below** any user-facing
application that wants to advertise "verified agent" semantics.
The composition order, top to bottom:

```
  user application (CLI / API / IDE plugin / agent harness)
         │
         ▼
  v57 Verified Agent  ← single named artifact, single command
         │
         ├── v53  σ-governed harness loop (TAOR + abstain)
         ├── v54  σ-proconductor (multi-LLM σ-routing)
         ├── v55  σ₃-speculative decoding (per-channel + EARS)
         ├── v56  σ-Constitutional (verifier + IP-TTT + grokking + ANE)
         │
         ├── v48  execution sandbox + 7-layer defense
         ├── v47  Frama-C ACSL σ-kernel surface
         │
         ├── v49  DO-178C-aligned assurance pack
         └── v34  σ decomposition (epistemic / aleatoric)
```

Anything above v57 reuses the verified composition.  Anything
below v57 is verified independently and continues to ship its own
self-tests.  v57's job is to make the union *queryable*: one
table, one tier-tag, one command.

## See also

- `docs/v57/ARCHITECTURE.md` — composition map and tier semantics
- `docs/v57/POSITIONING.md` — vs ad-hoc sandboxing approaches
- `docs/v57/paper_draft.md` — position paper draft
- `creation.md` — project invariants
- `docs/WHAT_IS_REAL.md` — repo-wide tier table

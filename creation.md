# creation.md — Project invariants, not instructions

> **v53 design rule:** `claude.md` is a box of instructions. `creation.md` is
> a box of **invariants**. Instructions drift; invariants don't. An agent
> harness that enforces invariants needs fewer rules over time, not more.

## Invariants (these cannot be violated)

1. **σ gates every loop.** No agent loop may run without a σ-threshold
   set. If σ exceeds the threshold, the loop **abstains** — it does not
   try harder, it does not retry, it does not fall back to a heuristic.
2. **Fail-closed on unknown tools.** A sandbox with a policy list must
   deny any tool not in that list. No "benefit of the doubt" path.
3. **Network calls are explicit.** No module under `src/` may issue a
   network call unless the call site has an explicit comment and an env
   guard (`COS_ENABLE_NETWORK=1`-style). None today do.
4. **Determinism before throughput.** Every flagship and lab self-test
   must be deterministic. If a test depends on wall-clock, it must pin
   the seed / mock the clock.
5. **`make merge-gate` is the truth.** If it is red, nothing else in
   this repo counts. Claims in docs that outrun merge-gate are lab-tier
   and must be tagged M/P in [`docs/WHAT_IS_REAL.md`](docs/WHAT_IS_REAL.md).
6. **Tier discipline in prose.** Every capability claim must reference
   either (a) a self-test count (e.g. `check-v51 13/13`), (b) a formal
   target (`make verify`, `make certify`), or (c) an explicit `P-tier`
   tag. No blended claims.
7. **Reproducibility.** Binary audits (`scripts/v49/binary_audit.sh`)
   must see zero hardcoded secrets, zero surprise network symbols,
   reproducible builds.

## Conventions (style, not rule)

- Prefer **C11** for the teaching kernel and σ math. Extensions
  (Metal / SME / MLX) live in opt-in labs, never in the merge-gate.
- Figures follow the register in [`docs/VISUAL_INDEX.md`](docs/VISUAL_INDEX.md)
  (Spektre blue `#1e50a0`, Palatino-family serif for docs UI).
- Commit messages: `feat(vN): description` · `fix(vN): description` ·
  `docs(vN): description`. Never push without explicit user consent.

## Project memory (auto-updated — do not edit by hand)

- Canonical repo: [`spektre-labs/creation-os`](https://github.com/spektre-labs/creation-os)
- Last landed milestone: **v54** (σ-proconductor scaffold)
- Live merge-gate: `make merge-gate` (`check` + `check-v6` … `check-v29`)
- Live optional labs: `check-v31`, `check-v33` … `check-v48`, `check-v51`,
  `check-v53`, `check-v54`, `check-mcp`, `formal-sby-v37`, `verify`, `red-team`,
  `certify`, `v50-benchmark`
- Evidence: `docs/WHAT_IS_REAL.md`, `docs/CLAIM_DISCIPLINE.md`,
  `docs/COMMON_MISREADINGS.md`

## σ-profile for this project

| Task type        | σ ceiling (total) | Action above ceiling                   |
|------------------|-------------------|----------------------------------------|
| Exploration      | 0.80              | abstain, ask human for a narrower goal |
| Implementation   | 0.50              | abstain, ask for a failing test first  |
| Verification     | 0.20              | abstain, ask for a witness / proof     |
| Agent loop       | 0.75              | abstain, return partial progress       |
| Sub-agent spawn  | 0.60              | only spawn if main σ > 0.60 in domain  |

## What an agent must do when invariants conflict

- **Conflict between a convention and an invariant** → invariant wins.
- **Conflict between two invariants** → abstain, surface the conflict,
  ask the human to resolve. Do not pick one silently.
- **A user request to violate an invariant** → refuse and cite this
  file. (Example: "skip `make certify`" → refuse; cite invariant 5.)

## Pointers (not a second README)

- Architecture: [`docs/v51/ARCHITECTURE.md`](docs/v51/ARCHITECTURE.md),
  [`docs/v53/ARCHITECTURE.md`](docs/v53/ARCHITECTURE.md),
  [`docs/v54/ARCHITECTURE.md`](docs/v54/ARCHITECTURE.md)
- Positioning vs Claude Code: [`docs/v53/POSITIONING.md`](docs/v53/POSITIONING.md)
- Positioning vs MoA / RouteLLM / MoMA / Bayesian Orchestration: [`docs/v54/POSITIONING.md`](docs/v54/POSITIONING.md)
- Paper draft (σ-proconductor for multi-LLM): [`docs/v54/paper_draft.md`](docs/v54/paper_draft.md)
- Paper draft (σ-governance vs time-limits):
  [`docs/v53/paper_draft.md`](docs/v53/paper_draft.md)
- Tier tags: [`docs/WHAT_IS_REAL.md`](docs/WHAT_IS_REAL.md)
- Claim discipline: [`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md)

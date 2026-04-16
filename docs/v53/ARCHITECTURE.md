# Creation OS v53 — σ-governed harness architecture

> **Tier (honest):** integration scaffold (C) + interpretive critique (I).
> v53 is a **structural critique** of the Claude Code harness, expressed
> in small C surfaces + this architecture note + a paper draft. It is
> **not** a Claude Code clone, **not** a running coding agent, and **not**
> a product claim. Measured artifact is `make check-v53` (13/13 self-test).
> Tier tags live in [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md).

## What this lab is (and is not)

- **Is:** four small, auditable C surfaces that encode σ-governance rules
  that a future coding agent runtime could enforce:
  1. `src/v53/harness/loop.{c,h}` — σ-TAOR loop with five distinct abstain
     reasons (σ, drift, no-progress, no-tool-calls, budget).
  2. `src/v53/harness/dispatch.{c,h}` — σ-triggered sub-agent dispatch
     (spawn only when main σ in a domain ≥ trigger; specialist runs in
     fresh context; result returned as summary + its own σ).
  3. `src/v53/harness/compress.{c,h}` — σ-prioritized message scoring
     (learning moments + invariant refs + tool-use outrank routine filler).
  4. `src/v53/harness/project_context.{c,h}` — conservative `creation.md`
     loader (counts invariants, conventions, σ-profile rows).
- **Is not:** a message bus, a tool executor, a sub-process manager, a
  tokenizer, or a model runtime. v53 does not call `claude` / any
  external API. It is a **policy surface**, not a runtime.

## The one-line difference from Claude Code

```
Claude Code nO:    while (tool_call)                               { … }
Creation OS v53:   while (tool_call && σ < threshold
                                    && σ_ewma < drift_cap
                                    && making_progress)            { … }
```

| Claude Code pattern                         | v53 counterpart                                      |
|---------------------------------------------|------------------------------------------------------|
| `nO` single-threaded master loop            | `v53_taor_step` + `v53_harness_state_t`              |
| `h2A` async message queue                   | *(scaffold)* single flat list; no queue abstraction  |
| `TAOR` Think-Act-Observe-Repeat             | five-outcome TAOR w/ σ-abstain + drift + no-progress |
| Capability primitives (Read/Write/Exec)     | *(scaffold)* caller supplies a tool table            |
| `wU2` compressor (92% autocompact)          | `v53_compress_*` σ-qualitative scoring               |
| Sub-agent spawn (manual)                    | `v53_dispatch_if_needed` σ-triggered auto-dispatch    |
| `claude.md` project instructions            | `creation.md` project **invariants**                 |
| Plan Mode (architectural alignment)         | creation.md §σ-profile + invariant-conflict rule     |
| Fork agents (shared prefix, 95% token save) | *(not yet; scaffold single-agent)*                   |
| 4-layer compression (temporal)              | `v53_compress_*` σ-quality percentile cutoff         |

## Abstain reasons (fail-loud, not "try harder")

`v53_taor_step` returns one of:

| Outcome                     | Trigger                                            |
|-----------------------------|----------------------------------------------------|
| `V53_LOOP_CONTINUE`         | σ below threshold, progress rising, tool calls present |
| `V53_LOOP_COMPLETE`         | model returned no tool calls AND progress ≥ 1.0    |
| `V53_LOOP_ABSTAIN_SIGMA`    | σ_total > `sigma_threshold` this iteration         |
| `V53_LOOP_ABSTAIN_DRIFT`    | EWMA of σ > `drift_cap` (session-long drift)       |
| `V53_LOOP_ABSTAIN_NO_PROG`  | `no_progress_streak ≥ no_progress_cap`             |
| `V53_LOOP_ABSTAIN_NO_TOOLS` | model stopped without producing a tool call or goal completion |
| `V53_LOOP_BUDGET_EXHAUSTED` | iteration ≥ `max_iterations`                       |
| `V53_LOOP_SAFETY_BLOCK`     | reserved for a future defense-layer veto           |

All abstain outcomes are **terminal**. The loop does not retry, does not
"try anyway", and does not escalate silently.

## σ-triggered sub-agent dispatch — the Mama Claude insight

- **Main agent** runs the σ-TAOR loop on the full task.
- If main-agent σ in a **specific domain** (e.g. `security`) exceeds that
  domain's `spawn_trigger`, the runtime spawns a specialist in a
  **fresh, uncorrelated context** — this is the core Mama Claude
  observation: uncorrelated context *is* test-time compute.
- The specialist returns a **summary + its own σ**, not its raw trace,
  so the main agent does not re-pollute its context with every failed
  sub-step.
- If the specialist's σ is still above its own cap
  (`specialist_sigma_cap`), the dispatch result is marked
  `specialist_abstained = 1` — the main agent then has to pick between
  asking a human or broadening the goal.

## σ-prioritized compression — the qualitative axis

| Signal                    | Weight  | Why                                   |
|---------------------------|---------|---------------------------------------|
| σ resolved (high → low)   | **+2.0**| this is where learning happened       |
| References invariant      | **+1.5**| anchors to `creation.md`              |
| Tool calls made           | **+1.0**| grounded action, not just talk        |
| Peak σ of the message     | **+σ**  | informative difficulty                |
| Pure filler               | **−0.5**| drop before everything else           |

`v53_compress_batch` drops a percentile-cutoff share of messages; the
scaffold self-test checks that (a) learning + invariant-referencing
messages survive and (b) filler is dropped first.

## `creation.md` loader — invariants, not instructions

- The loader is a **conservative** markdown parser: it counts numbered
  bullets in `## Invariants`, dash bullets in `## Conventions`, and
  parses rows of the `## σ-profile` table into `(task, sigma_ceiling)`
  pairs.
- The self-test asserts ≥ 5 invariants, ≥ 1 convention, ≥ 3 σ-profile
  rows are present in the canonical `creation.md` at the repo root.
- If `creation.md` is missing / empty, `v53_load_project_context`
  returns `ok = 0`. A real harness would **refuse to start** in that
  case — invariants drive everything downstream.

## How v53 maps to upstream labs

| Concern                           | v53 surface                        | Upstream lab                     |
|-----------------------------------|------------------------------------|----------------------------------|
| σ-from-logits                     | (reused) v51 cognitive_loop        | v34 Dirichlet + v45 introspection |
| Sandbox / σ-deny                  | (reused) v51 agent sandbox         | v48 σ-armored red-team lab       |
| Verification of the loop itself   | future Frama-C contracts           | v47 ACSL σ-kernel + v49 certify   |
| Benchmark rollup of harness runs  | future `benchmarks/v53/…`          | v50 rollup harness               |

## Reproduce

```bash
make check-v53                       # 13/13 self-test
./creation_os_v53 --architecture     # banner only
```

## Pointers

- Positioning table: [`POSITIONING.md`](POSITIONING.md)
- Paper draft: [`paper_draft.md`](paper_draft.md)
- Invariants: [`../../creation.md`](../../creation.md)
- Tier discipline: [`../CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
- Tier tags: [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md)

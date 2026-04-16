# Harness Architecture: Why σ-Governance Beats Time-Limits in Agentic Coding

> **Paper draft — working title, not peer-reviewed.** Status: I-tier
> (interpretive) position paper. Companion scaffold: `src/v53/` +
> [`ARCHITECTURE.md`](ARCHITECTURE.md). Tier discipline applies:
> the claim below is structural; empirical comparison belongs to a
> follow-up paper with a real coding-agent benchmark.

## Abstract

Modern coding-agent harnesses (Claude Code and its peers) are built
around a simple control loop: while the model proposes a tool call,
execute it, observe the result, repeat. The harness is a *dumb* loop
that wraps a *smart* model. This architecture has produced remarkable
throughput, but it also produces runaway behaviour — loops that run
forever, sessions that quietly drift off the task, agents that
hallucinate tool arguments confidently. The public mitigations for
these failure modes are out-of-band: **weekly usage limits**, cost
caps, human review. We argue that these mitigations are symptoms of
one missing architectural piece: a *runtime-level* signal that the
model does not know what to do next. We call this signal σ
(coherence / uncertainty), and we introduce σ-governance — a design
in which the harness, not the model, holds the stop criterion. We
describe a σ-governed variant of the Think-Act-Observe-Repeat (TAOR)
loop, a σ-triggered sub-agent dispatch rule, a σ-qualitative context
compression policy, and an invariants file (`creation.md`) that
replaces `claude.md`-style instructions. We ship the design as a
scaffold in C with a deterministic self-test; empirical comparison
with a production agent is deliberately out of scope.

## 1. Background: what Claude Code gets right

Claude Code's harness (reverse-engineered in the community literature
as `nO` / `h2A` / TAOR / `wU2` / sub-agents / `claude.md` / Plan Mode /
fork agents / 4-layer compression) is exceptional work. Its primitives
are the right primitives for a coding agent: a single-threaded master
loop keeps state simple; `h2A` lets I/O overlap; TAOR composes well
with tool-call JSON; Bash as a universal adapter collapses the
capability surface from dozens of APIs to one shell; fork agents
share a prompt prefix to save tokens; 4-layer compression keeps
context useful past the nominal window.

This paper does not re-derive that design. We take it as given.

## 2. The gap: model-side stop criterion

The TAOR loop terminates when the model emits plain text without a
tool call. In practice this is a useful heuristic, but it is not a
safety property. The model may emit plain text because:

1. the task is genuinely complete; or
2. the task is partially complete and the model gave up silently; or
3. the model has lost track of the goal; or
4. the model is about to hallucinate a "done" message.

The harness cannot distinguish (1) from (2–4). The public mitigation
is an out-of-band usage cap — a *weekly* cap, not a *per-task* one —
because finer-grained limits would collide with the variance of real
coding work.

Our thesis: a weekly cap is a symptom. The missing architectural
piece is a **per-iteration** signal that reflects the model's own
uncertainty about the next step.

## 3. σ as a runtime signal

σ in Creation OS (v33–v51 labs, this repository) is a decomposed
uncertainty measurement: a normalized-entropy total, a Dirichlet-evidence
decomposition into aleatoric and epistemic components, and a family
of auxiliary channels (top-margin, stability, cross-head variance,
critical-token detection). σ is computed from logits, per token, in
SIMD with ≈0.17% overhead (v46 lab). For the loop-level argument in
this paper, any sufficiently-calibrated σ in [0, 1] will do — the
architectural claim does not depend on one specific σ-channel family.

The point of σ is that it is **structural**, not anecdotal. The
harness does not have to ask the model "are you sure?" (the model
will say yes). The harness reads σ off the distribution the model
just produced.

## 4. σ-governance

We define σ-governance by four rules:

**R1 — σ bounds every loop.** The TAOR loop exits with a named
abstain outcome (`ABSTAIN_SIGMA`, `ABSTAIN_DRIFT`, `ABSTAIN_NO_PROG`,
`ABSTAIN_NO_TOOLS`, `BUDGET_EXHAUSTED`) instead of running to a time
limit. Abstention is a first-class outcome.

**R2 — σ triggers sub-agent spawns.** When main-agent σ in a given
*domain* exceeds that domain's trigger, the runtime spawns a
specialist with a fresh uncorrelated context (Mama Claude: fresh
context is test-time compute) and receives back a summary + the
specialist's own σ. The main agent does not inherit the specialist's
trace.

**R3 — σ drives context compression.** Messages are scored by a
policy that weights (σ-resolution, invariant-reference, tool-use,
peak σ) above (routine filler). Compression is qualitative, not
temporal.

**R4 — invariants, not instructions.** Project context lives in
`creation.md` as a list of invariants with an explicit σ-profile
per task type. The harness refuses to start with a missing or empty
`creation.md`. Conflicts between an invariant and a user request
resolve toward the invariant.

## 5. Why this beats time-limits

A time-limit is a blunt instrument that optimizes for the expected
case. A σ-governed loop is a *structural* instrument that optimizes
for the worst case of each individual session:

- A session that is **going well** (σ low, progress rising) runs
  freely — no cap needed.
- A session that is **drifting** (EWMA of σ rising) stops early — no
  wasted hours.
- A session that **hits a domain gap** (σ spikes in one domain only)
  spawns a specialist — the problem is localized, not globalized.
- A session that **is producing filler** (low σ, no tool calls, no
  invariant references) is compressed away first — context stays
  useful.

None of these behaviors require a wall-clock cap. The cap is replaced
by a σ-shaped envelope that is tighter in the bad regions and looser
in the good ones.

## 6. Scope and limits of this paper

- This is a **design paper**. We ship a scaffold (`src/v53/`, ≈600 LoC
  C + tests) that encodes the σ-governance rules as auditable surfaces.
  We do **not** run a coding-agent benchmark against Claude Code in
  this paper. That belongs in a follow-up paper with a realistic
  task set, matched model weights, and matched tool runtimes.
- σ in this paper is any calibrated uncertainty signal in [0, 1];
  the specific channels are an implementation detail.
- σ-governance does not replace UX. A user still needs to see which
  abstain outcome occurred and why — but showing that is *easier*
  than explaining "weekly limits exceeded".

## 7. Relation to prior work

- **Selective prediction / conformal prediction**: these calibrate
  the abstention decision *after* the fact on held-out data. σ
  operates per-token, per-iteration, inside the session.
- **Self-verification / best-of-N**: σ can *trigger* best-of-N (v41
  budget forcing), but the loop-level gate in v53 is orthogonal to
  the sampling strategy.
- **Guard systems / input-output filters** (NeMo Guardrails, OpenAI
  moderation): those operate on content. σ-governance operates on
  the *statistical structure* of the distribution, independent of
  content topic.
- **Claude Code / Cursor agent mode / OpenAI Agents SDK**: these
  harnesses are the reference point. v53 is a structural critique,
  not a competitor product.

## 8. A testable prediction

If σ-governance is correct, then a harness that observes σ should
hit **fewer runaway sessions per unit task** at the cost of some
abstentions on borderline tasks. A correct benchmark would report:

- `accuracy_total` — fraction of tasks completed correctly
- `accuracy_answered` — fraction of *attempted* tasks completed correctly
- `abstention_rate` — fraction of tasks the harness refused to finish
- `runaway_rate` — fraction of tasks that exceeded a budget without
  a named abstain outcome

A σ-governed harness should strictly dominate a σ-blind harness on
`runaway_rate`. `accuracy_answered` and `abstention_rate` should
trade off along a σ-threshold curve. v53 ships the harness surface;
benchmarks belong to the follow-up.

## 9. Conclusion

Claude Code's architecture is a landmark. Its one missing piece is
a runtime-level uncertainty signal that lets the harness — not the
model — decide when to stop. σ-governance fills that piece with a
small, auditable surface: a σ-TAOR loop, σ-triggered sub-agent
dispatch, σ-qualitative compression, and invariants in place of
instructions. The claim in this paper is structural, not empirical.
The invitation is: if you have a coding-agent benchmark you trust,
add σ-governance as a second arm and tell us what happens.

## Reproduce

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make check-v53                           # 13/13 self-test
./creation_os_v53 --architecture         # banner only
cat creation.md                          # invariants
cat docs/v53/ARCHITECTURE.md             # scaffold map
cat docs/v53/POSITIONING.md              # side-by-side table
```

## Acknowledgements

This paper builds directly on public reverse-engineering of the Claude
Code harness by the agentic-coding community. Any misreading of their
system is the author's alone. The author thanks reviewers who insisted
on the "structural, not empirical" framing — it sharpened the paper.

## License

AGPL-3.0-or-later (same as the repository).

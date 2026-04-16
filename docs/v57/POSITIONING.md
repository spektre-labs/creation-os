# v57 Positioning — vs ad-hoc agent sandboxes

## The driving oivallus

The Q2 2026 agent-framework field's structural problem, restated:

> Open-source Claude-Code-style agents and their forks ship
> ad-hoc sandboxing — Docker containers, WASM sandboxes,
> tool-call allowlists, "best practices."  Some Rust rewrites
> exist; one is mature (WASM-based, capability-conscious),
> another vanished (404).  Enterprise products lean on cloud-side
> isolation.  None of them can answer a basic technical question:
>
>   "What is your agent provably unable to do?"
>
> The answer everyone offers is the same:  *trust our architecture*.

v57 is the response: don't trust — **verify**.

## Side-by-side (categories, not specific products)

The table below groups the field by category.  Specific product
names from the driving oivallus prompt (OpenClaw / IronClaw /
ZeroClaw / Claude Code) appear there as stylized references and
are not independently audited here.  The CVE counts and ARR
figures cited in that prompt are the prompt's reporting, not
ours.

| property                | ad-hoc OSS agent          | hardened OSS agent (Rust + WASM) | enterprise SaaS agent | **Creation OS v57**                          |
|-------------------------|---------------------------|----------------------------------|-----------------------|----------------------------------------------|
| license                 | permissive (e.g. MIT)     | permissive (e.g. Apache-2.0)     | proprietary           | AGPL-3.0                                     |
| primary language        | Node / TypeScript         | Rust                             | TypeScript / Go       | C11 + small shell driver                     |
| isolation               | opt-in container          | WASM sandbox + capability gates  | cloud-side, opaque    | formally bounded surface (sandbox + ACSL)    |
| reasoning trust model   | prompt + config           | prompt + config                  | prompt + config       | σ-gated + verifier-floored                   |
| multi-LLM strategy      | model-agnostic, routed    | model-agnostic, routed           | typically single-vendor | σ-triangulated proconductor (v54)         |
| self-modification model | none / external fine-tune | none / external fine-tune        | external fine-tune    | σ-Constitutional IP-TTT controller (v56)     |
| invariant registry      | n/a                       | n/a                              | n/a                   | 5 invariants, tiered M / F / I / P (v57)     |
| proof artifacts shipped | none                      | design claims                    | internal audit        | Frama-C ACSL + DO-178C-aligned + sby (tier-tagged) |
| user-facing claim       | "trust our architecture"  | "trust our architecture"         | "trust our SLA"       | "run `make verify-agent`"                    |
| failure mode if tooling missing | n/a (none)        | n/a (none)                       | opaque                | honest `SKIP`, never silent `PASS`           |

The single column where the user-facing answer is *verify* is v57.

## Why composition matters more than any single layer

Agent frameworks treat sandboxing, reasoning trust, multi-model
routing, and self-modification as four separate concerns.  In
practice an attack on one becomes an attack on the others:

- A capable sandbox + a model that confabulates tool calls
  → the sandbox executes the wrong things, perfectly.
- A σ-aware reasoning loop + a sandbox without a capability
  policy → the agent abstains correctly, then a single approved
  tool call leaks the universe.
- Both above + a router that picks between LLMs by *cost* and
  not by *epistemic fit* → wrong answers admitted with high
  confidence.

v57's contribution is the **composition table**: for each agent-
runtime concern, name the slot, name the owning Creation OS
version, and tier the claim.  Removing any slot from the table
makes the missing concern explicit.  Downgrading a slot's tier
makes the regression explicit.  Both make pretending impossible.

## Distance from competing approaches

### vs ad-hoc OSS agent frameworks

v57's `execution_sandbox` is the v48 sandbox — capability-typed,
deny-by-default, with a separate test surface (`make check-v48`)
that returns 0 only when the deterministic checks pass.  An
ad-hoc framework can match the runtime behaviour, but cannot
match the **explicit tier table** without doing the v33–v56
work that backs each row.

### vs hardened OSS agent frameworks (Rust + WASM)

The closest peers are also empirical.  v57's `harness_loop` is
σ-governed: the loop refuses to emit when σ exceeds the
configured threshold, before any sandbox check runs.  WASM is a
strong execution-layer answer; σ-governance is an orthogonal
reasoning-layer answer.  The two compose.

### vs enterprise SaaS agents

Enterprise systems trade transparency for SLA.  v57 trades SLA
for transparency.  AGPL-3.0 + a static, tier-tagged registry +
one shell script makes any third party able to reproduce the
verification report.  That is not a competitive market position;
it is the only honest position when "trust the architecture" is
the failure mode.

## What v57 deliberately does not claim

1. **It is not safer than every other framework.** It is more
   honestly tiered than every other framework.  Honesty about
   tier is a different axis from absolute safety.
2. **It is not faster than every other framework.** It is
   smaller than every other framework.  Smaller surface is the
   precondition for tier honesty, not a marketing comparison.
3. **It is not a certified product.** v49's DO-178C artifacts
   are tier I.  Certification requires FAA / EASA, which is
   not on the project's roadmap.  The artifacts exist so that
   any team that *does* go through certification has a head
   start they did not have before.
4. **It does not eliminate the need for a sandbox.** It
   *requires* a sandbox (the v48 slot) and *additionally*
   requires a σ-gated reasoning loop on top of it.  Two layers,
   each tier-tagged, each independently verifiable.

## What would change the table

Promotions to F-tier require shipping mechanically-checked
proofs, not narrative.  The next concrete promotions on the
roadmap, in priority order:

1. `bounded_side_effects` → F via TLA+ specification of the
   sandbox-policy subset (see `planned` field in `src/v57/invariants.c`)
2. `sigma_gated_actions` → strictly F via expanded ACSL
   contracts on the v53 σ-TAOR surface (currently F only on
   the v47 σ-kernel surface)
3. `do178c_assurance_pack` slot → M via `make certify` with all
   assurance steps green on a CI host that ships the required
   tooling (Frama-C, sby, gcov, etc.)

When these land, the table updates.  Until they land, the table
stays honest.

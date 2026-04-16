# Creation OS v54 — Positioning vs. multi-LLM orchestration (2024–2026)

> **Tier (honest):** interpretive / positioning document (I-tier). This
> page compares v54's σ-proconductor policy with the recent multi-LLM
> orchestration literature. It does **not** claim v54 runs a live
> four-model ensemble today — see [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md)
> for what is measurable (`make check-v54` 14/14; no network).

## Reference systems

| System                   | Year | Routing signal                              | Aggregation                | Abstention |
|--------------------------|-----:|---------------------------------------------|----------------------------|------------|
| **MoA** (Mixture-of-Agents, ICLR'25) | 2024–25 | Layered reference-model responses           | Aggregator LLM combines    | —          |
| **RouteLLM** (ICLR'25)   | 2024–25 | Binary classifier: strong vs weak model     | Winner-takes-all           | —          |
| **MoMA** (Generalized routing LLMs + agents) | 2025 | Hybrid features, learned router | Router-chosen model         | —          |
| **FrugalGPT**            | 2023–24 | Cascade: try cheapest first; escalate      | Sequential, confidence gate| limited    |
| **LLMRouterBench / RouterArena** | 2026 | Benchmarks over existing routers | n/a (eval)                 | n/a        |
| **Bayesian Orchestration** | Jan 2026 | LLMs as likelihood functions              | Posterior over answers     | implicit   |
| **MoErging** ("Recycling and Routing Among Specialized Experts") | 2026 | Expert-specialization similarity | Specialist output          | —          |
| **Creation OS v54**      | 2026 | **σ-profile per domain** (measured over time) | **σ-weighted + disagreement abstain** | **explicit** |

None of the 2024–2026 systems routes on a per-query σ signal; none
abstains on pairwise disagreement among frontier models; none updates
its routing table from measured σ.

## Axes where v54 differs structurally

1. **Routing signal is σ, not question-type.** Classifiers look at
   the *text* of the question and guess a domain. σ-proconductor
   uses the *distribution* each model would produce — either
   predicted from a learned profile, or measured once the response
   returns. Profiles drift from reputation toward measurement.
2. **Aggregation is σ-weighted, not majority-vote.** σ_ensemble is
   the product of per-model σs. Three agreeing models with σ ≈ 0.20
   collapse to σ ≈ 0.008 — the multi-LLM version of v40's
   exponential-suppression threshold theorem. No vote counting.
3. **Disagreement is a first-class abstention reason.** Pairwise
   similarity below a threshold → `ABSTAIN_DISAGREE`. The system
   does not pick a side when frontier models disagree on a legal /
   medical / safety question.
4. **Profiles are learned from σ + (optional) ground truth.** EWMA
   per (agent, domain). Reputation becomes a slow-moving data
   artifact, not a config constant.
5. **Easy-query shortcut.** If any registered agent has σ_primary <
   0.10, only that agent is consulted. No credits burnt
   triangulating "what is the capital of Finland".
6. **Policy and dispatch are separate binaries.** v54 never reaches
   the network itself (creation.md invariant #3). The caller is the
   HTTP client. The scaffold is offline, deterministic, and testable.

## Where existing systems win today

- **Real production traffic** — RouteLLM, MoA, and MoMA run on live
  deployments; v54 is a policy scaffold with no API integration.
- **Polished UX** — OpenAI Agents / LangGraph / LiteLLM route across
  providers at scale with caching, retries, billing, and observability.
  v54 has none of these.
- **Established evaluation harnesses** — `LLMRouterBench`,
  `RouterArena`, and benchmark suites around FrugalGPT / Bayesian
  Orchestration are things v54 should eventually publish a comparable
  arm on, not compete with.

## Where v54 is structurally ahead

- **Abstention on disagreement** is the only part of this design that
  is intrinsically multi-LLM; a single-model API cannot do it.
- **σ-weighted aggregation** is the only combining rule in this
  literature that makes the *joint* confidence drop exponentially as
  independent agents agree — every other rule loses that property.
- **Learned σ-profiles** replace static router weights with a signal
  that updates from what the models actually do, not from a
  classifier that guesses what they will do.

## The testable claim

> Under matched tool / cost / latency budgets, a σ-proconductor can
> hit **strictly lower `runaway_rate` + `high_confidence_wrong_rate`**
> than a σ-blind router, at the cost of some abstentions on
> borderline questions. Every other axis (throughput, token cost,
> absolute accuracy) is an empirical trade-off to measure.

v54 ships the policy. Benchmarks belong to a follow-up paper.

## Pointers

- Architecture (this lab): [`ARCHITECTURE.md`](ARCHITECTURE.md)
- Paper draft: [`paper_draft.md`](paper_draft.md)
- Invariants file: [`../../creation.md`](../../creation.md)
- Tier tags: [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md)
- Outer harness: [`../v53/ARCHITECTURE.md`](../v53/ARCHITECTURE.md)

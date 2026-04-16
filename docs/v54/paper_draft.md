# σ-Proconductor: σ as the Missing Routing Signal for Multi-LLM Ensembles

> **Paper draft — working title, not peer-reviewed.** Status: I-tier
> (interpretive) position paper. Companion scaffold: `src/v54/` +
> [`ARCHITECTURE.md`](ARCHITECTURE.md). Tier discipline applies: the
> claim below is structural; empirical comparison on a real router
> benchmark belongs to a follow-up paper.

## Abstract

Multi-LLM orchestration in 2024–2026 has converged on a common shape:
a router chooses between frontier models using some combination of
question-type classification, historical accuracy, cost, and latency,
and an aggregator combines responses via majority vote, meta-model
scoring, or Bayesian posterior. We observe that none of the 2024–2026
routing systems — MoA, RouteLLM, MoMA, FrugalGPT, Bayesian
Orchestration, MoErging — uses the model's own **σ** (calibrated
per-query uncertainty) as the routing signal. We argue that σ is the
missing signal, and that adding it produces three capabilities the
existing literature cannot reproduce: (i) σ-weighted aggregation that
drops joint uncertainty exponentially as independent agents agree,
(ii) explicit abstention when frontier models disagree, and (iii)
routing tables that are *learned from observed σ* rather than
*guessed from question text*. We describe σ-proconductor — a
policy-only runtime in ≈600 lines of C — and ship it as a scaffold
with a deterministic, network-free self-test.

## 1. The state of multi-LLM orchestration

Multi-LLM orchestration has been one of the most active sub-areas of
applied LLM research since 2024. A representative (not exhaustive)
cluster of recent work:

- **MoA** (Mixture-of-Agents, ICLR 2025): layered agent responses,
  aggregator model combines; open-source stack beats GPT-4 Omni on
  AlpacaEval 2.0.
- **RouteLLM** (ICLR 2025): binary classifier selects between a
  strong and a weak model.
- **MoMA** (2025): generalized routing over LLMs *and* agents.
- **FrugalGPT** / **LLMRouterBench** / **RouterArena** (2026):
  benchmarks that standardize the routing-vs-quality trade-off.
- **Bayesian Orchestration** (January 2026): treats LLMs as
  likelihood functions; routing becomes posterior inference.
- **MoErging** ("Recycling and Routing Among Specialized Experts",
  2026): specialist-routing in an expert soup.

All of these produce a routing decision. **None of them reads σ off
the output distribution the model just produced.** The routing is
predicted from features available *before* the model runs (question
text, historical accuracy, cost, latency), or computed after the fact
from an external judge / classifier.

## 2. σ as a universal routing signal

σ in Creation OS is a calibrated, decomposed uncertainty: normalized
softmax entropy as the total, Dirichlet-evidence aleatoric / epistemic
components, and a family of auxiliary channels (top-margin, stability,
cross-head variance). σ is computable per token from logits with
≈0.17% overhead (v46 lab). For the orchestration argument in this
paper, any calibrated σ in [0, 1] suffices.

The σ signal does three things no current router uses:

1. **Sort.** Per-domain σ-profiles (learned by EWMA) are ranked
   routing keys. A router that sorts on σ-fit rather than
   classified-domain fit is strictly more informative if σ tracks
   actual difficulty.
2. **Triangulate.** σ_ensemble = Π σᵢ. If three independent
   frontier models all land at σ ≈ 0.2 on the same answer,
   σ_ensemble ≈ 0.008. This is the multi-LLM expression of v40's
   threshold theorem: *exponential suppression with independent
   channels*. A single-model API cannot produce this number.
3. **Abstain.** Pairwise disagreement among frontier models is a
   signal that at least one of them is wrong. A σ-proconductor
   treats disagreement > threshold as an explicit abstain outcome,
   not as an excuse to run a tiebreaker.

## 3. σ-proconductor: policy

The full policy fits in three functions and a type.

- `v54_subagent_t` holds a per-domain `sigma_profile[N_DOMAINS]`, a
  cost, a latency, and EWMA-updated observed accuracy.
- `v54_select_subagents()` scores agents as
  `0.7·(1−σ_primary) + 0.3·(1−σ_secondary) − cost/100 − latency/10000`
  and picks K by stakes (1 / 2 / 4) with an easy-query shortcut
  when `σ_primary < 0.10`.
- `v54_aggregate_responses()` returns one of five outcomes —
  `CONSENSUS`, `SIGMA_WINNER`, `ABSTAIN_SIGMA`, `ABSTAIN_DISAGREE`,
  `EMPTY` — computing σ_ensemble as the product of reported σs and
  pairwise disagreement as `1 − mean_similarity`.

The policy is deterministic, offline, and testable. The actual HTTP
call to `claude` / `gpt` / `gemini` / `deepseek` is **the caller's
job** — the scaffold never touches the network (creation.md
invariant #3).

## 4. σ-proconductor: learning

Reputation is a slow-moving data artifact, not a config constant.
`v54_learn_profile_update()` applies an EWMA with α = 0.05 to both
`sigma_profile[domain]` and `observed_accuracy[domain]`;
`v54_learn_from_aggregation()` attributes a ground-truth signal to the
winning agent (non-winners receive "unknown", so no false
reward/punishment).

Over time, the hand-tuned reference profiles in the scaffold are
overwritten by measured ones. Claude may turn out better at logic than
we initially estimated. GPT may drift weaker on creative than the
community consensus suggests. The router learns this from σ, not from
Twitter.

## 5. Relation to prior art

- **Mixture-of-Agents (MoA)** feeds reference-model responses into an
  aggregator LLM. A σ-proconductor can be combined with MoA by using
  σ-weighted voting *as* the aggregator — no separate aggregator
  model required.
- **RouteLLM / MoMA** are binary or policy-gradient routers; a
  σ-proconductor is a *continuous* routing policy where the weight is
  the inverse of σ.
- **Bayesian Orchestration** treats LLMs as likelihoods and computes
  a posterior. σ_ensemble = Π σᵢ is the frequentist cousin of that
  posterior when the likelihoods are independent; σ_proconductor's
  `ABSTAIN_DISAGREE` fires when the Bayesian posterior would be
  bimodal.
- **FrugalGPT** cascades by cost. σ-proconductor cascades by σ: you
  stop as soon as one agent's σ is low enough, *regardless* of
  whether it's the cheapest one.

## 6. Scope and limits of this paper

- **Design paper.** We ship a scaffold (`src/v54/`, ≈600 LoC C + 14
  deterministic tests) that encodes the σ-proconductor policy as
  auditable surfaces. We do **not** run v54 against Claude / GPT /
  Gemini / DeepSeek in this paper; that belongs in a follow-up with a
  matched router benchmark (LLMRouterBench, RouterArena).
- **σ is any calibrated uncertainty in [0, 1].** The specific σ-channel
  family is orthogonal to the orchestration argument.
- **Similarity metric is pluggable.** The scaffold uses Jaccard over
  tokens for reproducibility; a real runtime swaps in an embedding
  backend without touching the rest of the policy.

## 7. A testable prediction

If σ-proconductor is correct, a σ-observing router should strictly
dominate a σ-blind router on two measurable axes:

- **`runaway_rate`** — fraction of queries answered with high
  confidence by the system despite disagreement among the underlying
  agents. A σ-proconductor's ABSTAIN_DISAGREE drives this toward zero
  on ambiguous tasks.
- **`high_confidence_wrong_rate`** — fraction of wrong answers
  delivered at low reported σ. σ-ensemble / product collapses this on
  tasks where independent agents agree; explicit abstention bounds it
  on tasks where they don't.

Other axes — absolute accuracy, throughput, token cost, latency —
trade off along a threshold curve and are empirical questions for the
follow-up benchmark.

## 8. Conclusion

The 2024–2026 multi-LLM routing literature produced excellent routers
and excellent aggregators, but all of them operate on features
available *before* the model runs or *after* a separate judge scores
the output. σ — the model's own calibrated uncertainty — is the
signal none of them reads. A σ-proconductor uses σ to sort
(per-domain routing), to triangulate (σ-weighted aggregation with
exponential suppression), and to abstain (disagreement > threshold).
The claim in this paper is structural: σ is the missing routing
signal, and adding it unlocks capabilities a σ-blind system
architecturally cannot have. The invitation is: pick a router
benchmark you trust, add a σ-proconductor arm, and tell us what
happens.

## Reproduce

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
make check-v54                           # 14/14 self-test, no network
./creation_os_v54 --architecture         # banner only
cat docs/v54/ARCHITECTURE.md             # scaffold map
cat docs/v54/POSITIONING.md              # vs MoA / RouteLLM / MoMA / Bayesian
```

## Acknowledgements

This paper builds directly on the routing literature cited above. Any
misreading of those systems is the author's alone.

## License

AGPL-3.0-or-later (same as the repository).

# Creation OS v54 — σ-Proconductor architecture

> **Tier (honest):** integration scaffold (C) + interpretive positioning (I).
> v54 is the orchestration policy for multi-LLM ensembles, expressed as
> small auditable C surfaces + this doc + a paper draft. It is **not** a
> live multi-LLM client: **no network calls are made from inside
> `src/v54/`**. That is creation.md invariant #3; the split is enforced
> by design. Measured artifact: `make check-v54` (14/14 self-test).

## What this lab is (and is not)

- **Is:** four small C surfaces + reference subagent profiles that
  encode σ-proconductor policy:
  1. `src/v54/proconductor.{c,h}` — `v54_subagent_t`, `v54_proconductor_t`,
     registry, five hand-tuned reference profiles
     (`claude`, `gpt`, `gemini`, `deepseek`, `local_bitnet`).
  2. `src/v54/dispatch.{c,h}` — keyword-heuristic classifier,
     deterministic top-K selector with easy-query shortcut,
     σ-weighted aggregator with consensus / σ-abstain / disagreement-
     abstain outcomes.
  3. `src/v54/disagreement.{c,h}` — Jaccard-over-tokens similarity
     analyzer with outlier detection (embeddings-free on purpose).
  4. `src/v54/learn_profiles.{c,h}` — EWMA updater for per-domain σ +
     observed-accuracy, plus `v54_learn_from_aggregation()` helper.
- **Is not:** an HTTP client, embedding model, tokenizer, sub-process
  dispatcher, or billing layer. Callers fetch responses; this lab
  decides *who* to ask and *how* to combine.

## The one-line claim

```
MoA / RouteLLM / MoMA / Bayesian Orchestration:  route on question-type, cost, historical accuracy
v54 σ-proconductor:                              route on σ-profile per domain, triangulate on σ_ensemble,
                                                 abstain on pairwise disagreement
```

## Wire map (inside v54)

```
           ┌────────────────────────────────────────────────┐
query ───▶ │  v54_classify_query()                          │  keyword heuristic, no LLM
           │   → v54_query_profile_t                        │
           └───────────────────────┬────────────────────────┘
                                   ▼
           ┌────────────────────────────────────────────────┐
           │  v54_select_subagents()                        │  σ-profile fit −  cost/latency penalties
           │   → K agent indices                            │  stakes-scaled K ∈ {1,2,4};
           └───────────────────────┬────────────────────────┘  easy-query σ<0.10 → K=1
                                   ▼
           ┌────────────────────────────────────────────────┐
           │  CALLER DISPATCH  (outside src/v54/)           │  real API calls / local model
           │   → v54_response_t[]                            │  text + reported σ + latency + cost
           └───────────────────────┬────────────────────────┘
                                   ▼
           ┌────────────────────────────────────────────────┐
           │  v54_aggregate_responses()                     │  uses disagreement analyzer
           │   → v54_aggregation_t:                          │  σ_ensemble = Π σᵢ
           │      CONSENSUS / SIGMA_WINNER /                 │  CONSENSUS needs disagreement ≤ 0.20
           │      ABSTAIN_SIGMA / ABSTAIN_DISAGREE / EMPTY   │  ABSTAIN_DISAGREE needs disagreement > 0.50
           └───────────────────────┬────────────────────────┘
                                   ▼
           ┌────────────────────────────────────────────────┐
           │  v54_learn_from_aggregation()                  │  EWMA drift on σ-profile
           │   updates winner (or all) per-domain           │  + optional ground-truth calibration
           └────────────────────────────────────────────────┘
```

## Selection policy (deterministic)

For each registered subagent and a `v54_query_profile_t`:

```
fit           = 0.7 · (1 − σ_primary)  +  0.3 · (1 − σ_secondary)
cost_penalty  = cost_per_1k_tokens / 100
lat_penalty   = latency_p50_ms     / 10000
score         = fit − cost_penalty − lat_penalty
```

K is chosen by stakes:

| Stakes          | K                                            |
|-----------------|----------------------------------------------|
| `< 0.30`        | 1 (cheapest capable)                         |
| `0.30–0.70`     | 2 (pairwise verify)                          |
| `≥ 0.70`        | `min(4, max_parallel_queries, n_agents)`     |

**Easy-query shortcut:** if any agent has `σ_profile[primary] < 0.10`,
force K = 1 — don't burn credits triangulating a trivially-easy query.

## Aggregation outcomes

| Outcome                  | Trigger                                                     |
|--------------------------|-------------------------------------------------------------|
| `V54_AGG_EMPTY`          | no responses                                                |
| `V54_AGG_CONSENSUS`      | n > 1, pairwise disagreement ≤ 0.20, σ_ensemble ≤ threshold |
| `V54_AGG_ABSTAIN_SIGMA`  | σ_ensemble (or best σ for n=1) > `convergence_threshold`    |
| `V54_AGG_ABSTAIN_DISAGREE` | pairwise disagreement > `disagreement_abstain`            |
| `V54_AGG_SIGMA_WINNER`   | borderline disagreement; lowest-σ response wins             |

σ_ensemble is the product of reported σs. A consensus of 0.20 × 0.18 =
0.036 suppresses σ exponentially as agents agree — this is the
multi-LLM version of v40's threshold theorem (exponential suppression
with independent channels).

## Disagreement analyzer

- Lexical Jaccard over lowercased alphanumeric tokens (≤ 64 unique
  tokens per response). No embeddings, no tokenizer, no network.
- Outlier = response with lowest average similarity to others.
- A real runtime swaps `v54_pairwise_similarity()` for an embedding
  backend; every other surface is unchanged.

## Learner

- `v54_learn_profile_update()` applies an EWMA with α = 0.05 on both
  `sigma_profile[domain]` and `observed_accuracy[domain]`.
- `v54_learn_from_aggregation()` applies the update to the winner,
  optionally to all participants (non-winners receive an "unknown"
  ground-truth flag so no false reward/punishment).
- Persistence is the caller's job (e.g. an mmap'd `profiles.bin` —
  out of scope for the scaffold).

## How v54 plugs into v51/v53

- v54 is the **Execute** phase of the v51 cognitive loop when the task
  benefits from multiple frontier models, not just the local BitNet.
  The loop's "classify difficulty" stays in v51; v54 adds the
  "classify domain" axis and the K-way parallel plan.
- v53's σ-TAOR harness stays the outer contract. A harness iteration
  that calls v54 gets back a `v54_aggregation_t`; CONSENSUS /
  SIGMA_WINNER continues the loop; either ABSTAIN outcome triggers
  `V53_LOOP_ABSTAIN_SIGMA` / `ABSTAIN_DRIFT` naturally.

## Reference profiles (illustrative, not measured)

| Agent          | logic | factual | creative | code | meta | Notes                    |
|----------------|------:|--------:|---------:|-----:|-----:|--------------------------|
| `claude`       |  0.15 |   0.30  |    0.25  | 0.20 | 0.22 | tight on logic           |
| `gpt`          |  0.25 |   0.18  |    0.22  | 0.15 | 0.24 | strong on code + factual |
| `gemini`       |  0.30 |   0.20  |    0.18  | 0.28 | 0.26 | creative edge            |
| `deepseek`     |  0.20 |   0.28  |    0.35  | 0.15 | 0.25 | code-heavy               |
| `local_bitnet` |  0.45 |   0.40  |    0.50  | 0.45 | 0.50 | cheap baseline (2B)      |

These numbers are design-note hand-tuned. Production use must feed
measured σs from the learner over a benchmarked corpus — don't ship
these as claims.

## Reproduce

```bash
make check-v54                           # 14/14 self-test, no network
./creation_os_v54 --architecture         # banner only
```

## Pointers

- Positioning vs MoA / RouteLLM / MoMA / Bayesian Orchestration:
  [`POSITIONING.md`](POSITIONING.md)
- Paper draft: [`paper_draft.md`](paper_draft.md)
- Invariants: [`../../creation.md`](../../creation.md)
- Tier tags: [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md)
- Outer harness: [`../v53/ARCHITECTURE.md`](../v53/ARCHITECTURE.md)

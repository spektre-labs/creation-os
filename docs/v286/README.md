# v286 — σ-Interpretability

**Explainability from the σ signal itself.**

σ_product is a geometric mean of **eight** channels — entropy,
repetition, calibration, attention, logit, hidden, output,
aggregate.  v286 makes the decomposition a first-class artefact:
for every answer we know which channel dominated σ, how σ
decomposes across attention heads, how σ reacts to counterfactual
token removal, and a human-readable report (trust percentage +
explanation + recommendation) that is **EU AI Act Article 13
understandable to the user**.

## σ innovation (what σ adds here)

> **σ is the only signal that simultaneously answers "how
> uncertain?" AND "*why* uncertain?"** — decomposed per channel,
> per attention head, and per counterfactual token, all in the
> same measurement.  The report is an actionable human sentence,
> not a number.

## v0 manifests

Enumerated in [`src/v286/interpretability.h`](../../src/v286/interpretability.h);
pinned by [`benchmarks/v286/check_v286_interpretability_decomposition.sh`](../../benchmarks/v286/check_v286_interpretability_decomposition.sh).

### 1. σ decomposition scenarios (exactly 4 canonical rows)

| scenario_id       | top_channel    | cause                            |
|-------------------|----------------|----------------------------------|
| `low_confidence`  | `entropy`      | model does not know the answer   |
| `repetitive`      | `repetition`   | model is repeating itself        |
| `overconfident`   | `calibration`  | model is overconfident           |
| `distracted`      | `attention`    | model could not focus attention  |

Contract: 4 canonical scenarios in order; 4 DISTINCT top_channels
drawn from the 8-channel σ_product set; every row has a non-empty
cause string.

### 2. Attention attribution (exactly 3 canonical heads, τ_attn = 0.40)

`head_0 · head_1 · head_2`, each with `σ_head ∈ [0, 1]` and
`status ∈ {CONFIDENT, UNCERTAIN}`, rule
`σ_head ≤ τ_attn → CONFIDENT else UNCERTAIN`.

Contract: canonical names; at least one CONFIDENT AND at least one
UNCERTAIN.

### 3. Counterfactual σ (exactly 3 tokens, δ_critical = 0.10)

Each: `token_id`, `σ_with`, `σ_without`, `delta_sigma =
|σ_without − σ_with|`, `classification ∈ {CRITICAL, IRRELEVANT}`,
rule `delta_sigma > δ_critical → CRITICAL else IRRELEVANT`.

Contract: 3 rows; `delta_sigma` matches the defining formula on
every row; at least one CRITICAL AND at least one IRRELEVANT.

### 4. Human-readable report (exactly 3 canonical rows)

Each: `response_id`, `trust_percent ∈ [0, 100]`,
`explanation_present == true`, `recommendation_present == true`,
`eu_article_13_compliant == true`.

Contract: 3 rows; trust_percent in [0, 100] on every row;
explanation AND recommendation present on every row; EU AI Act
Article 13 compliance asserted on every row.

### σ_interpret (surface hygiene)

```
σ_interpret = 1 −
  (decomp_rows_ok + decomp_channels_distinct_ok +
   decomp_all_causes_ok + attn_rows_ok + attn_both_ok +
   cf_rows_ok + cf_both_ok + cf_delta_ok +
   report_rows_ok + report_all_compliant_ok +
   report_trust_range_ok) /
  (4 + 1 + 1 + 3 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
```

v0 requires `σ_interpret == 0.0`.

## Merge-gate contracts

- 4 decomposition rows canonical; 4 distinct top_channels; every
  cause non-empty.
- 3 attention heads canonical; status matches σ vs τ_attn;
  CONFIDENT and UNCERTAIN both fire.
- 3 counterfactual rows; delta_sigma formula holds within 1e-5;
  CRITICAL and IRRELEVANT both fire.
- 3 report rows; trust_percent ∈ [0, 100]; explanation AND
  recommendation AND EU AI Act Article 13 compliance on every row.
- `σ_interpret ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** types decomposition / attention /
  counterfactual / report as predicates — no live explainer UI, no
  benchmark calibration study, no auditor-facing export bundle.
- **v286.1 (named, not implemented)** is a live σ-explainer UI that
  turns per-response decomposition + attention + counterfactual
  data into a user-facing report, a calibration study on real
  benchmarks tying `σ_interpret` to actual error rate, and an
  auditor-facing export bundle mapped to EU AI Act Articles 13 and
  15.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

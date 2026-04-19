# v285 — σ-EU-AI-Act

**Regulatory fit for the European AI Act (2024/1689)**.

Article 15 mandates robustness, accuracy, and cybersecurity of
high-risk AI.  σ_product is per-answer accuracy; σ-log is the
audit trail; absence of an RLHF feedback loop simplifies
Article-52 documentation obligations.  v285 types the compliance
surface as v0 predicates so an auditor can verify each property
statically from the kernel's JSON output, regardless of which
downstream pipeline Creation OS is embedded in.

## σ innovation (what σ adds here)

> **σ is the only property simultaneously measuring Article-15
> accuracy per answer AND generating the Article-52 audit trail
> AND classifying Article-6 risk tier from the same signal** —
> one telemetry channel, three conformance axes.

## v0 manifests

Enumerated in [`src/v285/eu_ai_act.h`](../../src/v285/eu_ai_act.h);
pinned by [`benchmarks/v285/check_v285_eu_ai_act_compliance.sh`](../../benchmarks/v285/check_v285_eu_ai_act_compliance.sh).

### 1. Article 15 mapping (exactly 3 canonical rows)

`robustness · accuracy · cybersecurity`, each `sigma_mapped == true`
AND `audit_trail == true`.

Contract: canonical names in order; 3 distinct; every Article-15
property is both σ-mapped AND audit-trailed.

### 2. Article 52 transparency (exactly 3 canonical rows)

`training_docs · feedback_collection · qa_process`, each
`required_by_eu == true` AND `sigma_simplifies == true`.

Contract: canonical order; required AND simplified on every row —
σ-Constitutional eliminates RLHF feedback collection, so the
documentation burden collapses into the σ-log itself.

### 3. Risk tiers (exactly 4 canonical rows)

Cascade τ_low = 0.20, τ_medium = 0.50, τ_high = 0.80.

| tier | σ_risk | sigma_gate | hitl | audit | redundancy | controls_count |
|------|:---:|:---:|:---:|:---:|:---:|:---:|
| `low`       | 0.10 | ✓ |   |   |   | 1 |
| `medium`    | 0.35 | ✓ |   | ✓ |   | 2 |
| `high`      | 0.65 | ✓ | ✓ | ✓ |   | 3 |
| `critical`  | 0.90 | ✓ | ✓ | ✓ | ✓ | 4 |

Contract: canonical labels; `sigma_gate == true` on every tier;
`controls_count` strictly monotonic `1 → 2 → 3 → 4`; `critical`
has all 4 controls.

### 4. License / regulation stack (exactly 3 canonical rows)

| name | layer |
|------|-------|
| `scsl`       | `LEGAL`      |
| `eu_ai_act`  | `REGULATORY` |
| `sigma_gate` | `TECHNICAL`  |

All enabled AND composable.  Contract: 3 distinct names; 3 distinct
layers covering legal / regulatory / technical; all enabled AND all
composable.

### σ_euact (surface hygiene)

```
σ_euact = 1 −
  (art15_rows_ok + art15_all_mapped_ok + art15_all_audit_ok +
   art52_rows_ok + art52_all_required_ok +
   art52_all_simplifies_ok + risk_rows_ok +
   risk_all_sigma_ok + risk_monotone_ok +
   license_rows_ok + license_layers_distinct_ok +
   license_all_enabled_ok + license_all_composable_ok) /
  (3 + 1 + 1 + 3 + 1 + 1 + 4 + 1 + 1 + 3 + 1 + 1 + 1)
```

v0 requires `σ_euact == 0.0`.

## Merge-gate contracts

- 3 Art-15 rows canonical; `sigma_mapped` AND `audit_trail` on every
  row.
- 3 Art-52 rows canonical; `required_by_eu` AND `sigma_simplifies`
  on every row.
- 4 risk tiers canonical; `sigma_gate` on every tier;
  `controls_count` strictly monotonic 1→2→3→4; critical has 4
  controls.
- 3 license rows canonical; 3 distinct layers; all enabled; all
  composable.
- `σ_euact ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** types the compliance surface as predicates —
  no live audit pipeline, no legal-opinion memo, no certified
  retention policy.
- **v285.1 (named, not implemented)** is a live end-to-end audit
  pipeline that turns the σ-log into a conformant Article-15/52
  evidence bundle (including model / data sheets, σ_risk per use
  case, and a certified record-retention policy), plus a
  legal-opinion memo mapping every v0 contract row to the
  corresponding EU AI Act provision.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

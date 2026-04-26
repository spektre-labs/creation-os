# Field comparison — AUROC framing (claim-disciplined)

**Purpose:** a **single table** reviewers can reuse for papers or posts, with explicit **evidence classes** so arithmetic, lab demos, and harness numbers are not blended.

## Measured row (this repository)

| System | Method | AUROC | N | Notes |
|--------|--------|-------|---|--------|
| Creation OS (graded CSV) | σ-gate reporting on one graded run | **0.8123** | 50 prompts (43 / 7 split in report) | **Harness-style reporting** on `benchmarks/graded/graded_results.csv`; see `benchmarks/graded/RESULTS.md` (generated artifact). Host metadata belongs in a repro bundle per [REPRO_BUNDLE_TEMPLATE.md](../REPRO_BUNDLE_TEMPLATE.md). |

## Literature anchors (not repository evidence)

The rows below are **placeholders for external baselines**. Do not treat quoted numbers as maintained in git unless you attach the paper PDF, official leaderboard snapshot, and matching evaluation script revision.

| System (illustrative) | Method (illustrative) | AUROC (order-of-magnitude anchor) | N | Hardware |
|-----------------------|------------------------|-----------------------------------|---|----------|
| Community / survey work on verbalized confidence | verbalized probability | fill from primary citation | varies | cloud typical |
| SelfCheck-style checks | NLI or consistency probes | fill from primary citation | varies | cloud typical |
| Semantic-entropy style estimators | sampling-based disagreement | fill from primary citation | varies | cloud typical |

## How to extend honestly

1. Freeze **one** Creation OS AUROC line to a `make …` command + CSV + `uname` / SoC string in the repro bundle.
2. For each external system, add a footnote with **paper + version + split**; never merge microbench throughput into the same headline sentence as AUROC.

See [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) § forbidden merges.

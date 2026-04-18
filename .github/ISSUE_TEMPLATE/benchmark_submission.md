---
name: Benchmark submission — v143 suite format
about: Submit a new benchmark run (v143 σ-native suite format).
title: "[bench] <suite-name> on <hardware> — <one-line summary>"
labels: ["benchmark", "v143", "needs-triage"]
assignees: []
---

<!--
Every benchmark submission MUST follow docs/BENCHMARK_PROTOCOL.md
and docs/CLAIM_DISCIPLINE.md. Tier labels are non-negotiable:
  tier-0  deterministic synthetic fixture
  tier-1  archived live run
  tier-2  live run on live data at submission time
-->

## Submission metadata

| Field | Value |
|---|---|
| Suite | <truthfulqa_mc2 / arc_challenge / hellaswag / v143 σ-native / …> |
| Tier | <0 / 1 / 2> |
| Hardware | <CPU / GPU / RAM / OS> |
| Commit | <SHA of creation-os HEAD> |
| Model | <BitNet-b1.58-2B / other GGUF> |
| Seed | <integer> |
| Bonferroni N | <corrections family-wise α> |

## Result

Paste the full JSON artefact produced by `make check-v143` or the
v111 matrix replay (`benchmarks/v111/run_matrix.sh`). Do NOT
summarize; the maintainer will extract the table.

```json
{
  "kernel": "v143",
  ...
}
```

## Checklist

- [ ] The JSON artefact is attached verbatim.
- [ ] Tier is declared and correct.
- [ ] The run is reproducible: I list exact commands above.
- [ ] If tier-1 or tier-2, I have attached the repro bundle
      (`docs/REPRO_BUNDLE_TEMPLATE.md`).
- [ ] I am NOT merging the σ microbench throughput with the σ
      harness accuracy numbers in any single sentence.

## Summary

<!-- What changed and why — **English** for all merged prose per docs/LANGUAGE_POLICY.md -->

## Claim hygiene (required if touching benchmarks, README numbers, or frontier comparisons)

- [ ] I read [docs/CLAIM_DISCIPLINE.md](../docs/CLAIM_DISCIPLINE.md).
- [ ] I did **not** merge microbench throughput with harness (MMLU/ARC/etc.) in prose.
- [ ] If I changed **§7 / `make bench`** behavior or README ratio text, I updated [docs/BENCHMARK_PROTOCOL.md](../docs/BENCHMARK_PROTOCOL.md) or confirmed it still matches `core/gemm_vs_bsc.c`.
- [ ] If I cite new **measured** numbers, I can attach a repro bundle per [docs/REPRO_BUNDLE_TEMPLATE.md](../docs/REPRO_BUNDLE_TEMPLATE.md) (stdout + host + SHA).

## Build

- [ ] `make check` passes locally from repo root of this tree.

## Terms (optional)

- [ ] New jargon links to [docs/GLOSSARY.md](../docs/GLOSSARY.md) or defines once in README/docs.

## Language

- [ ] All new **committed** markdown and maintainer-facing comments are **English only** ([docs/LANGUAGE_POLICY.md](../docs/LANGUAGE_POLICY.md)).

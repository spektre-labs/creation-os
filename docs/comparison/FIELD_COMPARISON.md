# Field comparison — AUROC framing (claim-disciplined)

**Purpose:** a **single table** reviewers can reuse for papers or posts, with explicit **evidence classes** so arithmetic, lab demos, and harness numbers are not blended.

## Measured row (this repository)

| System | Method | AUROC | N | Hardware |
|--------|--------|-------|---|----------|
| Creation OS (graded CSV) | σ-gate reporting on one graded run | **0.8123** | 50 prompts (43 / 7 split in report) | **Harness-style reporting** on `benchmarks/graded/graded_results.csv`; see `benchmarks/graded/RESULTS.md` (generated artifact). Host metadata belongs in a repro bundle per [REPRO_BUNDLE_TEMPLATE.md](../REPRO_BUNDLE_TEMPLATE.md). |

## Literature anchors (not repository evidence)

The rows below are **external benchmarks** cited for integrator-facing comparison. They are **not** re-run or maintained as in-repo harness numbers unless you attach the primary citation, split, and frozen evaluation script revision per [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).

| System | Method | AUROC (reported) | N | Hardware |
|--------|--------|------------------|---|----------|
| Cacioli et al. (valid subset, verbalized confidence; illustrative mean) | verbalized | **0.624** | 524 | cloud (typical) |
| Cacioli et al. (invalid subset; illustrative mean) | verbalized | **0.357** | 524 | cloud (typical) |
| SelfCheckGPT-class (survey anchor) | NLI / consistency probes | **~0.70** | varies | cloud typical |
| Semantic entropy–class (survey anchor; e.g. Nature-style sampling) | sampling / disagreement | **~0.79** | varies | cloud typical |

### Notes (read before citing)

- **Creation OS N=50 is small**; a larger prompt set (for example 200 prompts) should be archived with the same CSV discipline before treating AUROC as stable.
- **Creation OS runs locally** on consumer-class hardware in the reported graded bundle; **no cloud API** is required for that artifact path.
- **Pure C** applies to the σ-gate **control plane** in this tree; local inference may still use an external OpenAI-compatible server (Ollama, llama-server, etc.) when enabled.
- Replace Cacioli / SelfCheck / semantic-entropy numbers with **your frozen citation row** (paper + version + split) before publication; do not merge microbench throughput into the same headline sentence as AUROC.

See [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md) and [EXTERNAL_EVIDENCE_AND_POSITIONING.md](../EXTERNAL_EVIDENCE_AND_POSITIONING.md) for claim hygiene.
